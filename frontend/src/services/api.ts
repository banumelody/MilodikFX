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
  /** What the interface delivered, before the input trim. */
  inputLevel: number;
  /** What the chain actually receives, after the input trim. */
  chainInputLevel: number;
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

export interface PresetMetadata {
  name: string;
  description: string;
  tags: string[];
  favourite: boolean;
  notes: string;
  savedAt: string;
}

export interface PresetsResponse {
  /** Names alone, for callers that only need to know what exists. */
  presets: string[];
  details: PresetMetadata[];
  selected: string;
}

export interface Scene {
  index: number;
  name: string;
  /** effectId -> on. Absent means the scene says nothing about that effect. */
  enabled: Record<string, boolean>;
  /** False for a slot nothing has been stored in yet. */
  populated: boolean;
}

export interface ScenesState {
  scenes: Scene[];
  /** -1 once the chain has been changed by hand and matches no slot. */
  active: number;
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

export interface NamLibraryResponse {
  models: string[];
  directory: string;
  /** False when NAM was not compiled in, or the CPU has no AVX2. */
  available: boolean;
  /** Why loading is disabled, when it is. Empty when available. */
  unavailableReason: string;
}

export const getNamLibrary = () => request<NamLibraryResponse>('/nam');

export const revealNamFolder = () =>
  request<{ revealed: string }>('/nam/reveal', { method: 'POST', body: '{}' });

export const importNam = (name: string, base64: string) =>
  request<{ name: string; models: string[] }>('/nam/import', {
    method: 'POST',
    body: JSON.stringify({ name, data: base64 }),
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

export const setPresetMetadata = (
  name: string,
  changes: Partial<Pick<PresetMetadata, 'description' | 'tags' | 'favourite' | 'notes'>>,
) =>
  request<PresetMetadata>('/presets/metadata', {
    method: 'POST',
    body: JSON.stringify({ name, ...changes }),
  });

export const exportPreset = (name: string) =>
  request<{ name: string; filename: string; data: string }>('/presets/export', {
    method: 'POST',
    body: JSON.stringify({ name }),
  });

export const importPreset = (name: string, data: string) =>
  request<PresetsResponse>('/presets/import', {
    method: 'POST',
    body: JSON.stringify({ name, data }),
  });

export interface HistoryState {
  canUndo: boolean;
  canRedo: boolean;
  undoDepth: number;
  redoDepth: number;
  /** Present on undo/redo, so the UI redraws from what the engine settled on. */
  effects?: EffectDescriptor[];
}

export const getHistory = () => request<HistoryState>('/history');

export const undoChange = () =>
  request<HistoryState>('/history/undo', { method: 'POST', body: '{}' });

export const redoChange = () =>
  request<HistoryState>('/history/redo', { method: 'POST', body: '{}' });

export const getScenes = () => request<ScenesState>('/scenes');

export const recallScene = (index: number) =>
  request<ScenesState>(`/scenes/${index}/recall`, { method: 'POST', body: '{}' });

export const captureScene = (index: number) =>
  request<ScenesState>(`/scenes/${index}/capture`, { method: 'POST', body: '{}' });

export const renameScene = (index: number, name: string) =>
  request<ScenesState>(`/scenes/${index}`, { method: 'PUT', body: JSON.stringify({ name }) });

export const setSceneEffect = (index: number, effect: string, enabled: boolean) =>
  request<ScenesState>(`/scenes/${index}/effects/${encodeURIComponent(effect)}`, {
    method: 'PUT',
    body: JSON.stringify({ enabled }),
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
function pollLevels(
  onLevels: (levels: Levels) => void,
  onError: (error: unknown) => void,
  intervalMs: number,
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

/**
 * Subscribes to the meters, preferring the engine's event stream.
 *
 * One held-open connection at 30 Hz instead of a fresh HTTP request every
 * 100 ms, which on a thread-per-connection server meant a thread per poll.
 *
 * Falls back to polling if the stream never delivers anything -- an environment
 * without EventSource, or a proxy that will not pass text/event-stream, must
 * still get meters rather than a dead display. Once the stream has worked, a
 * later error is a disconnection: EventSource reconnects by itself, so the only
 * thing to do is report it.
 */
export function subscribeLevels(
  onLevels: (levels: Levels) => void,
  onError: (error: unknown) => void,
  intervalMs = 100,
): () => void {
  if (typeof EventSource !== 'function') return pollLevels(onLevels, onError, intervalMs);

  let stopped = false;
  let delivered = false;
  let stopPolling: (() => void) | null = null;
  let source: EventSource | null = new EventSource(`${API_BASE}/levels/stream`);

  source.onmessage = (event) => {
    if (stopped) return;

    try {
      onLevels(JSON.parse(event.data) as Levels);
      delivered = true;
    } catch {
      /* a truncated frame; the next one is 33 ms away */
    }
  };

  source.onerror = () => {
    if (stopped) return;

    onError(new Error('Aliran meter terputus'));

    if (delivered) return;

    // It never worked at all, so retrying it forever would leave the meters
    // dead. Close it and go back to polling.
    source?.close();
    source = null;

    if (!stopPolling) stopPolling = pollLevels(onLevels, onError, intervalMs);
  };

  return () => {
    stopped = true;
    source?.close();
    stopPolling?.();
  };
}
