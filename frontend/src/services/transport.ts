/**
 * How a request reaches the engine.
 *
 * The same React app runs in two very different places:
 *
 * - **The standalone app**, where the engine serves this bundle from a loopback
 *   HTTP server and `fetch` works exactly as it looks.
 * - **The VST3 plugin**, where there is no server at all. A plugin instance must
 *   not open a socket — several instances in one project would fight over ports,
 *   and a DAW has no business hosting a web server. JUCE's WebBrowserComponent
 *   serves the bundle straight out of the plugin binary and exposes a native
 *   function bridge instead.
 *
 * The bridge is deliberately shaped like an HTTP call — method, path, query,
 * body, and a status back — so the entire typed client above it, and the whole
 * REST layer in C++ below it, are shared verbatim. `RestApiDispatcher` never
 * knew it was talking to a socket in the first place.
 *
 * The protocol is JUCE's own (`__juce__invoke` / `__juce__complete`), spelled
 * out here rather than imported from JUCE's `juce.js`: that module is served by
 * the WebView at runtime, and importing it would mean this bundle could no
 * longer be built and embedded as one self-contained file.
 */

/** What the engine answered, whichever way the question got there. */
export interface TransportResponse {
  status: number;
  body: string;
}

export type Transport = (
  method: string,
  path: string,
  query: string,
  body: string,
) => Promise<TransportResponse>;

interface JuceBackend {
  addEventListener: (event: string, handler: (payload: unknown) => void) => void;
  emitEvent: (event: string, payload: unknown) => void;
}

interface JuceBridge {
  backend?: JuceBackend;
  initialisationData?: { __juce__functions?: string[] };
}

/** The one native function the plugin registers; see PluginEditor.cpp. */
const NATIVE_FUNCTION = 'milodikfxApi';

function juceBridge(): JuceBridge | undefined {
  if (typeof window === 'undefined') return undefined;
  return (window as unknown as { __JUCE__?: JuceBridge }).__JUCE__;
}

/** True when this bundle is running inside the plugin's WebView. */
export function isPluginHost(): boolean {
  const bridge = juceBridge();
  return (
    bridge?.backend != null &&
    (bridge.initialisationData?.__juce__functions ?? []).includes(NATIVE_FUNCTION)
  );
}

function createNativeTransport(backend: JuceBackend): Transport {
  const pending = new Map<number, (result: unknown) => void>();
  let nextId = 0;

  backend.addEventListener('__juce__complete', (payload) => {
    const { promiseId, result } = (payload ?? {}) as { promiseId?: number; result?: unknown };
    if (promiseId == null) return;
    const resolve = pending.get(promiseId);
    if (!resolve) return;
    pending.delete(promiseId);
    resolve(result);
  });

  return (method, path, query, body) =>
    new Promise<TransportResponse>((resolve) => {
      const resultId = nextId++;

      pending.set(resultId, (result) => {
        // The backend always answers with { status, body }. Anything else means
        // the bridge itself misbehaved, which is worth surfacing as a 502 rather
        // than as an unhandled exception halfway up the typed client.
        const answer = result as Partial<TransportResponse> | undefined;

        resolve({
          status: typeof answer?.status === 'number' ? answer.status : 502,
          body: typeof answer?.body === 'string' ? answer.body : '',
        });
      });

      backend.emitEvent('__juce__invoke', {
        name: NATIVE_FUNCTION,
        params: [method, path, query, body],
        resultId,
      });
    });
}

function createFetchTransport(): Transport {
  // The engine falls back through ports 3000-3008, so the origin is read from
  // the page rather than hardcoded -- a fixed port would break exactly when the
  // fallback was needed. The `/api` prefix is already part of the path, because
  // that is the path the C++ dispatcher matches on either transport.
  const base = window.location.origin;

  return async (method, path, query, body) => {
    const response = await fetch(`${base}${path}${query ? `?${query}` : ''}`, {
      method,
      headers: { 'Content-Type': 'application/json' },
      body: method === 'GET' || method === 'DELETE' ? undefined : body,
    });

    return { status: response.status, body: await response.text() };
  };
}

let cached: Transport | null = null;

/** The transport for this host, chosen once. */
export function transport(): Transport {
  if (cached) return cached;

  const backend = juceBridge()?.backend;

  cached = isPluginHost() && backend ? createNativeTransport(backend) : createFetchTransport();
  return cached;
}

/**
 * Server-sent events only exist over HTTP. In the plugin there is no stream, so
 * callers fall back to polling -- which `subscribeLevels` already does when a
 * stream delivers nothing, so nothing new had to be invented for it.
 */
export function supportsEventStream(): boolean {
  return !isPluginHost() && typeof EventSource !== 'undefined';
}
