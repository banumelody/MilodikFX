import { messageBridge, DeviceMessage } from './messageBridge';
import { eventDispatcher } from './eventDispatcher';

export interface PresetState {
  name: string;
  processors: Record<string, Record<string, number>>;
}

export interface AudioMetrics {
  cpuLoad: number;
  inputLevel: number;
  outputLevel: number;
  inputPeak: number;
  outputPeak: number;
}

export class AudioEngineService {
  async getDevices(): Promise<DeviceMessage> {
    return messageBridge.getDeviceList();
  }

  async setInputDevice(deviceId: string): Promise<void> {
    await messageBridge.setInputDevice(deviceId);
    eventDispatcher.emit('deviceChanged', { type: 'input', deviceId });
  }

  async setOutputDevice(deviceId: string): Promise<void> {
    await messageBridge.setOutputDevice(deviceId);
    eventDispatcher.emit('deviceChanged', { type: 'output', deviceId });
  }

  async setParameter(processor: string, param: string, value: number): Promise<void> {
    await messageBridge.setParameter(processor, param, value);
    eventDispatcher.emit('parameterChanged', { processor, param, value });
  }

  async getParameter(processor: string, param: string): Promise<number> {
    // This would be implemented when the DSP engine provides this endpoint
    return new Promise((resolve) => {
      eventDispatcher.once(`parameter:${processor}:${param}`, resolve);
    });
  }

  async getPresets(): Promise<PresetState[]> {
    // This would be implemented when the DSP engine provides this endpoint
    return [];
  }

  async savePreset(preset: PresetState): Promise<void> {
    await messageBridge.savePreset(preset.name, preset);
    eventDispatcher.emit('presetSaved', preset);
  }

  async loadPreset(presetId: string): Promise<void> {
    await messageBridge.loadPreset(presetId);
    eventDispatcher.emit('presetLoaded', { id: presetId });
  }

  async deletePreset(presetId: string): Promise<void> {
    await messageBridge.deletePreset(presetId);
    eventDispatcher.emit('presetDeleted', { id: presetId });
  }

  getAudioMetrics(): Promise<AudioMetrics> {
    return new Promise((resolve) => {
      eventDispatcher.once('audioMetrics', resolve);
    });
  }

  subscribeToMetrics(callback: (metrics: AudioMetrics) => void): () => void {
    messageBridge.on('monitor', callback);
    return () => messageBridge.off('monitor', callback);
  }

  subscribeToParameters(callback: (data: any) => void): () => void {
    messageBridge.on('parameter', callback);
    return () => messageBridge.off('parameter', callback);
  }
}

export const audioEngine = new AudioEngineService();
