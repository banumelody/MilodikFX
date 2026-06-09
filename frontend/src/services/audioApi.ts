/**
 * AudioAPI Service - Frontend to Backend REST API communication layer
 * Handles all HTTP requests to the C++ backend audio engine.
 */

const API_BASE_URL = 'http://localhost:3000/api';

export interface Device {
  input: string[];
  output: string[];
  selectedInput: string;
  selectedOutput: string;
  sampleRate: number;
  bufferSize: number;
}

export interface Parameter {
  [key: string]: number | boolean | string;
}

export interface Levels {
  inputLevel: number;
  outputLevel: number;
}

export interface PresetInfo {
  presets: string[];
}

/**
 * Fetch devices available on the system
 */
export async function getDevices(): Promise<Device> {
  const response = await fetch(`${API_BASE_URL}/devices`, {
    method: 'GET',
    headers: { 'Content-Type': 'application/json' },
  });

  if (!response.ok) {
    throw new Error(`Failed to fetch devices: ${response.statusText}`);
  }

  return response.json();
}

/**
 * Select audio input/output device
 */
export async function selectDevice(
  input?: string,
  output?: string
): Promise<{ success: boolean; message: string }> {
  const body: { input?: string; output?: string } = {};
  if (input) body.input = input;
  if (output) body.output = output;

  const response = await fetch(`${API_BASE_URL}/devices/select`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  });

  if (!response.ok) {
    throw new Error(`Failed to select device: ${response.statusText}`);
  }

  return response.json();
}

/**
 * Get all current parameters
 */
export async function getParameters(): Promise<{ [key: string]: any }> {
  const response = await fetch(`${API_BASE_URL}/parameters`, {
    method: 'GET',
    headers: { 'Content-Type': 'application/json' },
  });

  if (!response.ok) {
    throw new Error(`Failed to fetch parameters: ${response.statusText}`);
  }

  return response.json();
}

/**
 * Set master volume (in dB)
 */
export async function setMasterVolume(value: number): Promise<{ value: number; applied: boolean }> {
  const response = await fetch(`${API_BASE_URL}/parameters/master-volume`, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ value }),
  });

  if (!response.ok) {
    throw new Error(`Failed to set master volume: ${response.statusText}`);
  }

  return response.json();
}

/**
 * Set effect parameter
 * @param effect Effect name (e.g., 'overdrive', 'eq', 'comp')
 * @param param Parameter name (e.g., 'drivePct', 'bassDb')
 * @param value The value to set
 */
export async function setEffectParameter(
  effect: string,
  param: string,
  value: number
): Promise<{ success: boolean }> {
  const response = await fetch(
    `${API_BASE_URL}/parameters/${effect}/${param}`,
    {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ value }),
    }
  );

  if (!response.ok) {
    throw new Error(
      `Failed to set effect parameter: ${response.statusText}`
    );
  }

  return response.json();
}

/**
 * Toggle effect on/off
 */
export async function toggleEffect(
  effect: string,
  enabled: boolean
): Promise<{ success: boolean }> {
  const response = await fetch(
    `${API_BASE_URL}/effects/${effect}/enabled`,
    {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ enabled }),
    }
  );

  if (!response.ok) {
    throw new Error(
      `Failed to toggle effect: ${response.statusText}`
    );
  }

  return response.json();
}

/**
 * Get real-time levels
 * Can be polled or WebSocket-based in future
 */
export async function getLevels(): Promise<Levels> {
  const response = await fetch(`${API_BASE_URL}/levels`, {
    method: 'GET',
    headers: { 'Content-Type': 'application/json' },
  });

  if (!response.ok) {
    throw new Error(`Failed to fetch levels: ${response.statusText}`);
  }

  return response.json();
}

/**
 * Subscribe to real-time level updates
 * @param callback Callback function called with new levels
 * @param interval Poll interval in ms (default 100ms for ~10Hz update rate)
 * @returns Cleanup function to stop polling
 */
export function subscribeLevels(
  callback: (levels: Levels) => void,
  interval: number = 100
): () => void {
  const timerId = setInterval(async () => {
    try {
      const levels = await getLevels();
      callback(levels);
    } catch (err) {
      console.error('Error fetching levels:', err);
    }
  }, interval);

  return () => clearInterval(timerId);
}

/**
 * Get available presets
 */
export async function getPresets(): Promise<PresetInfo> {
  const response = await fetch(`${API_BASE_URL}/presets`, {
    method: 'GET',
    headers: { 'Content-Type': 'application/json' },
  });

  if (!response.ok) {
    throw new Error(`Failed to fetch presets: ${response.statusText}`);
  }

  return response.json();
}

/**
 * Save current settings as a preset
 */
export async function savePreset(name: string): Promise<{ success: boolean; message: string }> {
  const response = await fetch(`${API_BASE_URL}/presets/save`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ name }),
  });

  if (!response.ok) {
    throw new Error(`Failed to save preset: ${response.statusText}`);
  }

  return response.json();
}

/**
 * Load a preset
 */
export async function loadPreset(name: string): Promise<{ success: boolean; message: string }> {
  const response = await fetch(`${API_BASE_URL}/presets/load`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ name }),
  });

  if (!response.ok) {
    throw new Error(`Failed to load preset: ${response.statusText}`);
  }

  return response.json();
}
