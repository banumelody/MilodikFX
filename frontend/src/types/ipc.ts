/**
 * IPC Message Types for MilodikFX Electron ↔ React Communication
 * 
 * This file defines all message structures for inter-process communication
 * between the Electron main process and React renderer process.
 * 
 * Usage in React:
 *   import { AudioEngineAPI } from './types/ipc'
 *   await window.audioEngine.setParameter('overdrive', 'drive', 0.75)
 * 
 * Usage in Electron:
 *   ipcMain.handle('setParameter', (event, data: SetParameterRequest) => {...})
 */

// ============================================
// COMMON TYPES
// ============================================

/**
 * Standard response format for IPC messages
 */
export interface IpcResponse<T = any> {
  success: boolean;
  data?: T;
  error?: {
    code: string;
    message: string;
  };
}

// ============================================
// PARAMETER CONTROL MESSAGES
// ============================================

/**
 * Request to set a parameter on an effect
 * Example: Set overdrive drive to 0.75
 */
export interface SetParameterRequest {
  effect: string;      // e.g., 'overdrive', 'eq', 'gain'
  parameter: string;   // e.g., 'drive', 'tone', 'level'
  value: number;       // 0.0 - 1.0 or effect-specific range
}

export type SetParameterResponse = IpcResponse<void>;

/**
 * Request to get current parameter value
 */
export interface GetParameterRequest {
  effect: string;
  parameter: string;
}

export interface GetParameterResponse extends IpcResponse {
  data?: number;
}

// ============================================
// DEVICE MANAGEMENT MESSAGES
// ============================================

/**
 * Audio device information
 */
export interface AudioDevice {
  id: string;
  name: string;
  isInput: boolean;
  sampleRate?: number;
  bufferSize?: number;
}

/**
 * Response containing list of devices
 */
export interface GetDevicesResponse extends IpcResponse {
  data?: AudioDevice[];
}

/**
 * Request to switch active device
 */
export interface SetDeviceRequest {
  deviceId: string;
  isInput?: boolean;  // Default: true (input device)
}

export type SetDeviceResponse = IpcResponse<void>;

// ============================================
// PRESET MANAGEMENT MESSAGES
// ============================================

/**
 * Effect parameter state for presets
 */
export interface EffectState {
  [effectName: string]: {
    [paramName: string]: number;
  };
}

/**
 * Complete preset data
 */
export interface PresetData {
  name: string;
  timestamp: number;
  effects: EffectState;
  metadata?: {
    author?: string;
    description?: string;
    tags?: string[];
  };
}

/**
 * Request to save current state as preset
 */
export interface SavePresetRequest {
  name: string;
  data: EffectState;
}

export interface SavePresetResponse extends IpcResponse {
  data?: {
    id: string;
    timestamp: number;
  };
}

/**
 * Request to load a preset
 */
export interface LoadPresetRequest {
  id: string;
}

export interface LoadPresetResponse extends IpcResponse {
  data?: PresetData;
}

// ============================================
// METER/MONITORING MESSAGES
// ============================================

/**
 * Real-time meter data (sent continuously)
 * Units: dB (-60 to 0, where 0 = full scale)
 */
export interface MeterData {
  inputLevel: number;   // Input level in dB
  outputLevel: number;  // Output level in dB
  peakLeft: number;     // Peak in left channel
  peakRight: number;    // Peak in right channel
  spectrum?: number[];  // Optional: frequency spectrum data
}

// ============================================
// STATUS/INFO MESSAGES
// ============================================

/**
 * Application status information
 */
export interface AppStatus {
  isAudioRunning: boolean;
  currentDevice?: AudioDevice;
  sampleRate: number;
  bufferSize: number;
  cpuLoad: number;  // CPU usage 0-100
}

/**
 * Error event from backend
 */
export interface AudioErrorEvent {
  code: string;
  message: string;
  severity: 'warning' | 'error' | 'critical';
  timestamp: number;
}

// ============================================
// CHANNEL NAMES (Constants)
// ============================================

export const IPC_CHANNELS = {
  // Invoke (request-response)
  SET_PARAMETER: 'setParameter',
  GET_PARAMETER: 'getParameter',
  GET_DEVICES: 'getDevices',
  SET_DEVICE: 'setDevice',
  SAVE_PRESET: 'savePreset',
  LOAD_PRESET: 'loadPreset',
  GET_STATUS: 'getStatus',
  
  // Events (one-way)
  METER_UPDATE: 'meterUpdate',
  PARAMETER_CHANGED: 'parameterChanged',
  DEVICE_CHANGED: 'deviceChanged',
  AUDIO_ERROR: 'audioError',
  
  // Testing
  PING: 'ping',
  PONG: 'pong',
} as const;

// ============================================
// TYPED WINDOW INTERFACE
// ============================================

/**
 * Types for window.audioEngine API
 * Used in React components with TypeScript strict mode
 */
export interface AudioEngineAPI {
  setParameter: (effect: string, parameter: string, value: number) => Promise<SetParameterResponse>;
  getParameter: (effect: string, parameter: string) => Promise<GetParameterResponse>;
  getDevices: () => Promise<GetDevicesResponse>;
  setDevice: (deviceId: string, isInput?: boolean) => Promise<SetDeviceResponse>;
  savePreset: (name: string, data: EffectState) => Promise<SavePresetResponse>;
  loadPreset: (id: string) => Promise<LoadPresetResponse>;
  onMeterUpdate: (callback: (data: MeterData) => void) => void;
  offMeterUpdate: () => void;
}

/**
 * Extended Window interface for TypeScript
 * Usage: const audioEngine = window.audioEngine as unknown as AudioEngineAPI
 */
declare global {
  interface Window {
    audioEngine: AudioEngineAPI;
    ipcRenderer: any;  // From preload.js
  }
}

// ============================================
// VALIDATION HELPERS
// ============================================

/**
 * Validate parameter value is in valid range
 */
export function validateParameterValue(value: number): boolean {
  return typeof value === 'number' && value >= 0 && value <= 1.0;
}

/**
 * Validate meter data has all required fields
 */
export function validateMeterData(data: any): data is MeterData {
  return (
    typeof data === 'object' &&
    typeof data.inputLevel === 'number' &&
    typeof data.outputLevel === 'number' &&
    typeof data.peakLeft === 'number' &&
    typeof data.peakRight === 'number'
  );
}

/**
 * Validate device object has required fields
 */
export function validateDevice(device: any): device is AudioDevice {
  return (
    typeof device === 'object' &&
    typeof device.id === 'string' &&
    typeof device.name === 'string' &&
    typeof device.isInput === 'boolean'
  );
}

export default {
  IPC_CHANNELS,
  validateParameterValue,
  validateMeterData,
  validateDevice,
};
