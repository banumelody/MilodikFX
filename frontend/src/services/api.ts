/**
 * Typed client for the MilodikFX engine's REST API.
 *
 * The base URL is derived from the page origin: the engine serves this bundle
 * itself, and it falls back to ports 3001+ when 3000 is taken, so a hardcoded
 * port would silently break exactly when the fallback was needed.
 */

const API_BASE = `${window.location.origin}/api`;

export interface ParameterDescriptor {
  id: string;
  label: string;
  unit: string;
  min: number;
  max: number;
  step: number;
  default: number;
  type: 'float' | 'bool';
  value: number;
}

export interface EffectDescriptor {
  id: string;
  label: string;
  description: string;
  enabled: boolean;
  /** False for stages that are always in the path (input routing, master out). */
  toggleable: boolean;
  parameters: ParameterDescriptor[];
}

export interface EffectsResponse {
  effects: EffectDescriptor[];
}

export interface Levels {
  inputLevel: number;
  outputLevel: number;
  gateGain: number;
  compressorReductionDb: number;
  limiterReductionDb: number;
  cpuPercent: number;
  sampleRate: number;
  bufferSize: number;
  audioRunning: boolean;
  floorDb: number;
}

export interface DeviceState {
  open: boolean;
  type: string;
  inputDevice: string;
  outputDevice: string;
  sampleRate: number;
  bufferSize: number;
  inputChannels: number;
  outputChannels: number;
  inputLatencyMs: number;
  outputLatencyMs: number;
  roundTripLatencyMs: number;
  lowLatency: boolean;
  error?: string;
}

export interface DeviceTypeInfo {
  name: string;
  lowLatency: boolean;
  inputs: string[];
  outputs: string[];
}

export interface DevicesResponse {
  current: DeviceState;
  available: {
    types: DeviceTypeInfo[];
    currentType: string;
    availableSampleRates: number[];
    availableBufferSizes: number[];
  };
}

export interface DeviceRequest {
  type?: string;
  inputDevice?: string;
  outputDevice?: string;
  sampleRate?: number;
  bufferSize?: number;
}

export interface PresetsResponse {
  presets: string[];
  selected: string;
}

export class ApiError extends Error {
  constructor(message: string, readonly status: number) {
    super(message);
    this.name = 'ApiError';
  }
}

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const response = await fetch(`${API_BASE}${path}`, {
    headers: { 'Content-Type': 'application/json' },
    ...init,
  });

  if (!response.ok) {
    // The engine answers with { "error": "..." }; surface that rather than a
    // bare status code, since it usually says exactly what the driver refused.
    let detail = response.statusText;

    try {
      const body = await response.json();
      if (body && typeof body.error === 'string') detail = body.error;
    } catch {
      /* the body was not JSON; the status text will have to do */
    }

    throw new ApiError(detail, response.status);
  }

  return response.json() as Promise<T>;
}

export const getEffects = () => request<EffectsResponse>('/effects');

export const getLevels = () => request<Levels>('/levels');

export const getDevices = () => request<DevicesResponse>('/devices');

export const setDevice = (body: DeviceRequest) =>
  request<{ current: DeviceState }>('/devices', {
    method: 'POST',
    body: JSON.stringify(body),
  });

/** Re-runs the engine's low-latency device search from scratch. */
export const optimiseDevice = () =>
  request<{ current: DeviceState }>('/devices/optimise', {
    method: 'POST',
    body: '{}',
  });

export const setParameter = (effect: string, parameter: string, value: number) =>
  request<{ effect: string; parameter: string; value: number }>(
    `/effects/${encodeURIComponent(effect)}/${encodeURIComponent(parameter)}`,
    { method: 'PUT', body: JSON.stringify({ value }) },
  );

export const setEffectEnabled = (effect: string, enabled: boolean) =>
  request<{ effect: string; enabled: boolean }>(
    `/effects/${encodeURIComponent(effect)}/enabled`,
    { method: 'POST', body: JSON.stringify({ enabled }) },
  );

export const getPresets = () => request<PresetsResponse>('/presets');

export const savePreset = (name: string) =>
  request<{ success: boolean; name: string }>('/presets/save', {
    method: 'POST',
    body: JSON.stringify({ name }),
  });

export const loadPreset = (name: string) =>
  request<{ success: boolean; name: string; effects: EffectDescriptor[] }>('/presets/load', {
    method: 'POST',
    body: JSON.stringify({ name }),
  });

export const deletePreset = (name: string) =>
  request<PresetsResponse>('/presets/delete', {
    method: 'POST',
    body: JSON.stringify({ name }),
  });

/**
 * Polls the meters, skipping a tick while the previous request is still in
 * flight so a slow response cannot pile requests up on the thread-per-connection
 * server. Returns an unsubscribe function.
 */
export function subscribeLevels(
  onLevels: (levels: Levels) => void,
  onError: (error: unknown) => void,
  intervalMs = 100,
): () => void {
  let stopped = false;
  let inFlight = false;

  const timer = window.setInterval(async () => {
    if (stopped || inFlight) return;

    inFlight = true;

    try {
      const levels = await getLevels();
      if (!stopped) onLevels(levels);
    } catch (error) {
      if (!stopped) onError(error);
    } finally {
      inFlight = false;
    }
  }, intervalMs);

  return () => {
    stopped = true;
    window.clearInterval(timer);
  };
}
