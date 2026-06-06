import { io, Socket } from 'socket.io-client';

export interface ParameterMessage {
  processor: string;
  param: string;
  value: number;
}

export interface PresetMessage {
  action: 'save' | 'load' | 'delete';
  name?: string;
  id?: string;
  state?: any;
}

export interface DeviceMessage {
  inputs: string[];
  outputs: string[];
  selectedInput?: string;
  selectedOutput?: string;
}

export interface MonitorMessage {
  cpuLoad: number;
  inputLevel: number;
  outputLevel: number;
  inputPeak: number;
  outputPeak: number;
}

export class MessageBridge {
  private socket: Socket | null = null;
  private listeners: Map<string, Function[]> = new Map();
  private connected = false;

  constructor(private serverUrl: string = 'http://localhost:3000') {}

  connect(): Promise<void> {
    return new Promise((resolve, reject) => {
      try {
        this.socket = io(this.serverUrl, {
          autoConnect: true,
          reconnection: true,
          reconnectionDelay: 1000,
          reconnectionDelayMax: 5000,
          reconnectionAttempts: 5,
        });

        this.socket.on('connect', () => {
          this.connected = true;
          this.setupListeners();
          resolve();
        });

        this.socket.on('disconnect', () => {
          this.connected = false;
        });

        this.socket.on('error', (error) => {
          console.error('Socket.IO error:', error);
          reject(error);
        });

        // Set a timeout for connection attempt
        setTimeout(() => {
          if (!this.connected) {
            reject(new Error('Connection timeout'));
          }
        }, 5000);
      } catch (error) {
        reject(error);
      }
    });
  }

  disconnect(): void {
    if (this.socket) {
      this.socket.disconnect();
      this.connected = false;
    }
  }

  isConnected(): boolean {
    return this.connected;
  }

  private setupListeners(): void {
    if (!this.socket) return;

    this.socket.on('parameter', (msg) => this.emit('parameter', msg));
    this.socket.on('device', (msg) => this.emit('device', msg));
    this.socket.on('preset', (msg) => this.emit('preset', msg));
    this.socket.on('monitor', (msg) => this.emit('monitor', msg));
  }

  async setParameter(processor: string, param: string, value: number): Promise<void> {
    return new Promise((resolve, reject) => {
      if (!this.socket || !this.connected) {
        reject(new Error('Not connected to audio engine'));
        return;
      }

      this.socket.emit('parameter', { processor, param, value }, (ack: any) => {
        if (ack?.error) {
          reject(new Error(ack.error));
        } else {
          resolve();
        }
      });
    });
  }

  async getDeviceList(): Promise<DeviceMessage> {
    return new Promise((resolve, reject) => {
      if (!this.socket || !this.connected) {
        reject(new Error('Not connected to audio engine'));
        return;
      }

      this.socket.emit('getDevices', {}, (ack: any) => {
        if (ack?.error) {
          reject(new Error(ack.error));
        } else {
          resolve(ack);
        }
      });
    });
  }

  async setInputDevice(deviceId: string): Promise<void> {
    return new Promise((resolve, reject) => {
      if (!this.socket || !this.connected) {
        reject(new Error('Not connected to audio engine'));
        return;
      }

      this.socket.emit('setInputDevice', { deviceId }, (ack: any) => {
        if (ack?.error) {
          reject(new Error(ack.error));
        } else {
          resolve();
        }
      });
    });
  }

  async setOutputDevice(deviceId: string): Promise<void> {
    return new Promise((resolve, reject) => {
      if (!this.socket || !this.connected) {
        reject(new Error('Not connected to audio engine'));
        return;
      }

      this.socket.emit('setOutputDevice', { deviceId }, (ack: any) => {
        if (ack?.error) {
          reject(new Error(ack.error));
        } else {
          resolve();
        }
      });
    });
  }

  async savePreset(name: string, state: any): Promise<void> {
    return new Promise((resolve, reject) => {
      if (!this.socket || !this.connected) {
        reject(new Error('Not connected to audio engine'));
        return;
      }

      this.socket.emit('preset', { action: 'save', name, state }, (ack: any) => {
        if (ack?.error) {
          reject(new Error(ack.error));
        } else {
          resolve();
        }
      });
    });
  }

  async loadPreset(id: string): Promise<void> {
    return new Promise((resolve, reject) => {
      if (!this.socket || !this.connected) {
        reject(new Error('Not connected to audio engine'));
        return;
      }

      this.socket.emit('preset', { action: 'load', id }, (ack: any) => {
        if (ack?.error) {
          reject(new Error(ack.error));
        } else {
          resolve();
        }
      });
    });
  }

  async deletePreset(id: string): Promise<void> {
    return new Promise((resolve, reject) => {
      if (!this.socket || !this.connected) {
        reject(new Error('Not connected to audio engine'));
        return;
      }

      this.socket.emit('preset', { action: 'delete', id }, (ack: any) => {
        if (ack?.error) {
          reject(new Error(ack.error));
        } else {
          resolve();
        }
      });
    });
  }

  async sendMessage(messageType: string, data: any): Promise<any> {
    return new Promise((resolve, reject) => {
      if (!this.socket || !this.connected) {
        reject(new Error('Not connected to audio engine'));
        return;
      }

      this.socket.emit(messageType, data, (ack: any) => {
        if (ack?.error) {
          reject(new Error(ack.error));
        } else {
          resolve(ack);
        }
      });
    });
  }

  on(event: string, callback: Function): void {
    if (!this.listeners.has(event)) {
      this.listeners.set(event, []);
    }
    this.listeners.get(event)!.push(callback);
  }

  off(event: string, callback: Function): void {
    const callbacks = this.listeners.get(event);
    if (callbacks) {
      const index = callbacks.indexOf(callback);
      if (index > -1) {
        callbacks.splice(index, 1);
      }
    }
  }

  private emit(event: string, data: any): void {
    this.listeners.get(event)?.forEach((cb) => cb(data));
  }
}

export const messageBridge = new MessageBridge();
