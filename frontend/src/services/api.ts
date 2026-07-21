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
  type: 'float' | 'bool' | 'text';
  /** A number for float/bool parameters, a name for text ones. */
  value: number | string;
  /** Choices offered for a text parameter, e.g. the impulse responses on disk. */
  options?: string[];
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

export const setParameter = (effect: string, parameter: string, value: number | string) =>
  request<{ effect: string; parameter: string; value: number | string }>(
    `/effects/${encodeURIComponent(effect)}/${encodeURIComponent(parameter)}`,
    { method: 'PUT', body: JSON.stringify({ value }) },
  );

export interface IrLibraryResponse {
  cabinets: string[];
  reverbs: string[];
  cabinetDirectory: string;
  reverbDirectory: string;
}

export const getIrLibrary = () => request<IrLibraryResponse>('/ir');

/** Opens the impulse-response folder in Explorer so files can be dropped in. */
export const revealIrFolder = (category: 'cabinet' | 'reverb') =>
  request<{ revealed: string }>('/ir/reveal', {
    method: 'POST',
    body: JSON.stringify({ category }),
  });

export const importIr = (category: 'cabinet' | 'reverb', name: string, base64: string) =>
  request<{ name: string; cabinets: string[]; reverbs: string[] }>('/ir/import', {
    method: 'POST',
    body: JSON.stringify({ category, name, data: base64 }),
  });

export interface TunerReading {
  enabled: boolean;
  /** Empty when nothing is detected. */
  note: string;
  midiNote: number;
  frequency: number;
  /** Deviation from the nearest semitone, -50..+50. */
  cents: number;
  confidence: number;
  /** The engine's own verdict on whether the reading is worth showing. */
  detected: boolean;
}

export const getTuner = () => request<TunerReading>('/tuner');

export const setTunerEnabled = (enabled: boolean) =>
  request<TunerReading>('/tuner/enable', {
    method: 'POST',
    body: JSON.stringify({ enabled }),
  });

/**
 * Polls the tuner while it is open. Same skip-while-in-flight rule as the
 * meters; 60 ms is fast enough for the needle to feel attached to the string.
 */
export function subscribeTuner(
  onReading: (reading: TunerReading) => void,
  onError: (error: unknown) => void,
  intervalMs = 60,
): () => void {
  let stopped = false;
  let inFlight = false;

  const timer = window.setInterval(async () => {
    if (stopped || inFlight) return;

    inFlight = true;

    try {
      const reading = await getTuner();
      if (!stopped) onReading(reading);
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

export type MidiMappingMode = 'continuous' | 'toggle';

export interface MidiMapping {
  /** -1 for the pending learn target, which has no controller yet. */
  cc: number;
  effect: string;
  parameter: string;
  mode: MidiMappingMode;
}

export interface MidiState {
  devices: string[];
  current: string;
  open: boolean;
  mappings: MidiMapping[];
  /** The parameter waiting to be bound, or null when learn is not armed. */
  learning: MidiMapping | null;
  /** Last control change seen, so a silent setup can be told from an unmapped one. */
  lastCc: number;
  lastValue: number;
}

export const getMidi = () => request<MidiState>('/midi');

export const setMidiDevice = (name: string) =>
  request<MidiState>('/midi/device', { method: 'POST', body: JSON.stringify({ name }) });

export const setMidiMapping = (
  cc: number,
  effect: string,
  parameter: string,
  mode: MidiMappingMode,
) =>
  request<MidiState>(`/midi/mappings/${cc}`, {
    method: 'PUT',
    body: JSON.stringify({ effect, parameter, mode }),
  });

export const clearMidiMapping = (cc: number) =>
  request<MidiState>(`/midi/mappings/${cc}`, { method: 'DELETE' });

/** Arms MIDI learn. Passing no target disarms it. */
export const learnMidi = (target?: { effect: string; parameter: string; mode: MidiMappingMode }) =>
  request<MidiState>('/midi/learn', {
    method: 'POST',
    body: JSON.stringify(target ?? {}),
  });

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
