import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

/**
 * The transport is chosen once and cached, so every case here re-imports the
 * module after arranging the environment it should detect.
 */
async function loadTransport() {
  vi.resetModules();
  return import('../transport');
}

interface Emitted {
  name: string;
  params: unknown[];
  resultId: number;
}

/** A stand-in for the bridge JUCE injects into the plugin's WebView. */
function installJuceBridge(functions = ['milodikfxApi']) {
  const listeners = new Map<string, (payload: unknown) => void>();
  const emitted: Emitted[] = [];

  (window as unknown as { __JUCE__: unknown }).__JUCE__ = {
    initialisationData: { __juce__functions: functions },
    backend: {
      addEventListener: (event: string, handler: (payload: unknown) => void) => {
        listeners.set(event, handler);
      },
      emitEvent: (event: string, payload: unknown) => {
        if (event === '__juce__invoke') emitted.push(payload as Emitted);
      },
    },
  };

  return {
    emitted,
    /** Answers the most recent invocation the way the C++ side would. */
    complete(result: unknown, index = emitted.length - 1) {
      listeners.get('__juce__complete')?.({ promiseId: emitted[index].resultId, result });
    },
  };
}

afterEach(() => {
  delete (window as unknown as { __JUCE__?: unknown }).__JUCE__;
  vi.unstubAllGlobals();
});

describe('transport', () => {
  describe('in the standalone app', () => {
    beforeEach(() => {
      vi.stubGlobal('fetch', vi.fn());
    });

    it('uses fetch against the page origin', async () => {
      const { transport, isPluginHost } = await loadTransport();

      expect(isPluginHost()).toBe(false);

      vi.mocked(fetch).mockResolvedValue({
        status: 200,
        text: async () => '{"ok":true}',
      } as unknown as Response);

      const response = await transport()('GET', '/api/effects', '', '');

      expect(fetch).toHaveBeenCalledWith(
        `${window.location.origin}/api/effects`,
        expect.objectContaining({ method: 'GET' }),
      );
      expect(response).toEqual({ status: 200, body: '{"ok":true}' });
    });

    it('appends a query string and omits a body on GET', async () => {
      const { transport } = await loadTransport();

      vi.mocked(fetch).mockResolvedValue({ status: 200, text: async () => '{}' } as unknown as Response);

      await transport()('GET', '/api/presets', 'details=1', '');

      const [url, init] = vi.mocked(fetch).mock.calls[0];
      expect(String(url)).toBe(`${window.location.origin}/api/presets?details=1`);
      expect((init as RequestInit).body).toBeUndefined();
    });

    it('keeps the event stream, which only exists over HTTP', async () => {
      // jsdom has no EventSource of its own, so stand one up: the point of this
      // case is the host check, not the browser's feature set.
      vi.stubGlobal('EventSource', class {});

      const { supportsEventStream } = await loadTransport();
      expect(supportsEventStream()).toBe(true);
    });
  });

  describe('inside the plugin', () => {
    it('routes through the native bridge instead of opening a socket', async () => {
      const bridge = installJuceBridge();
      vi.stubGlobal('fetch', vi.fn());

      const { transport, isPluginHost } = await loadTransport();

      expect(isPluginHost()).toBe(true);

      const pending = transport()('PUT', '/api/effects/reverb/mixPct', '', '{"value":30}');

      expect(bridge.emitted).toHaveLength(1);
      expect(bridge.emitted[0].name).toBe('milodikfxApi');
      expect(bridge.emitted[0].params).toEqual([
        'PUT',
        '/api/effects/reverb/mixPct',
        '',
        '{"value":30}',
      ]);

      bridge.complete({ status: 200, body: '{"value":30}' });

      await expect(pending).resolves.toEqual({ status: 200, body: '{"value":30}' });

      // A plugin instance must never open a socket: several instances in one
      // project would fight over ports.
      expect(fetch).not.toHaveBeenCalled();
    });

    it('matches each answer to its own request', async () => {
      const bridge = installJuceBridge();
      const { transport } = await loadTransport();
      const send = transport();

      const first = send('GET', '/api/effects', '', '');
      const second = send('GET', '/api/scenes', '', '');

      // Answered out of order, which is exactly what a slower first call does.
      bridge.complete({ status: 200, body: '"scenes"' }, 1);
      bridge.complete({ status: 200, body: '"effects"' }, 0);

      await expect(first).resolves.toEqual({ status: 200, body: '"effects"' });
      await expect(second).resolves.toEqual({ status: 200, body: '"scenes"' });
    });

    it('reports a malformed bridge answer as 502 rather than throwing', async () => {
      const bridge = installJuceBridge();
      const { transport } = await loadTransport();

      const pending = transport()('GET', '/api/effects', '', '');
      bridge.complete('not an object');

      await expect(pending).resolves.toEqual({ status: 502, body: '' });
    });

    it('falls back to polling even where EventSource exists', async () => {
      // The plugin's WebView is a real browser engine, so EventSource is
      // present -- there is simply no server behind it to stream from.
      vi.stubGlobal('EventSource', class {});
      installJuceBridge();

      const { supportsEventStream } = await loadTransport();

      expect(supportsEventStream()).toBe(false);
    });

    it('ignores a bridge that does not offer the API function', async () => {
      // JUCE defines a placeholder __JUCE__ even when nothing is registered.
      installJuceBridge([]);
      const { isPluginHost } = await loadTransport();

      expect(isPluginHost()).toBe(false);
    });
  });
});
