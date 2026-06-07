import React, { useState, useEffect } from 'react';
import { MeterData, AudioDevice, AudioEngineAPI } from '../types/ipc';

/**
 * IPC Test Component
 * 
 * Used to verify Electron ↔ React communication during development
 * Tests parameter updates, device selection, and meter monitoring
 * 
 * Features:
 * - Send parameter updates
 * - List audio devices
 * - Monitor meter data in real-time
 * - Display IPC communication status
 */

interface TestStatus {
  connected: boolean;
  lastMessage: string;
  meterUpdates: number;
  errorCount: number;
}

export const IpcTestComponent: React.FC = () => {
  const [status, setStatus] = useState<TestStatus>({
    connected: false,
    lastMessage: 'Waiting for connection...',
    meterUpdates: 0,
    errorCount: 0,
  });

  const [meterData, setMeterData] = useState<MeterData>({
    inputLevel: -60,
    outputLevel: -60,
    peakLeft: -60,
    peakRight: -60,
  });

  const [devices, setDevices] = useState<AudioDevice[]>([]);
  const [selectedDevice, setSelectedDevice] = useState<string>('default');

  const audioEngine = (window as any).audioEngine as AudioEngineAPI;

  // Initialize IPC connection and meter updates
  useEffect(() => {
    if (!audioEngine) {
      setStatus((prev) => ({
        ...prev,
        connected: false,
        lastMessage: 'audioEngine API not available - check preload.js',
        errorCount: prev.errorCount + 1,
      }));
      return;
    }

    setStatus((prev) => ({
      ...prev,
      connected: true,
      lastMessage: 'Connected to Electron IPC',
    }));

    // Subscribe to meter updates
    audioEngine.onMeterUpdate((data) => {
      setMeterData(data);
      setStatus((prev) => ({
        ...prev,
        meterUpdates: prev.meterUpdates + 1,
      }));
    });

    // Fetch device list
    loadDevices();

    return () => {
      audioEngine.offMeterUpdate?.();
    };
  }, []);

  const loadDevices = async () => {
    try {
      const response = await audioEngine.getDevices();
      if (response.success && response.data) {
        setDevices(response.data);
        setStatus((prev) => ({
          ...prev,
          lastMessage: `Loaded ${response.data!.length} audio devices`,
        }));
      }
    } catch (error) {
      setStatus((prev) => ({
        ...prev,
        lastMessage: `Error loading devices: ${error}`,
        errorCount: prev.errorCount + 1,
      }));
    }
  };

  const handleParameterUpdate = async (
    effect: string,
    parameter: string,
    value: number
  ) => {
    try {
      const response = await audioEngine.setParameter(effect, parameter, value);
      setStatus((prev) => ({
        ...prev,
        lastMessage: `Parameter updated: ${effect}.${parameter} = ${value}`,
      }));
    } catch (error) {
      setStatus((prev) => ({
        ...prev,
        lastMessage: `Error: ${error}`,
        errorCount: prev.errorCount + 1,
      }));
    }
  };

  const handleDeviceChange = async (deviceId: string) => {
    setSelectedDevice(deviceId);
    try {
      const response = await audioEngine.setDevice(deviceId);
      setStatus((prev) => ({
        ...prev,
        lastMessage: `Device changed to: ${deviceId}`,
      }));
    } catch (error) {
      setStatus((prev) => ({
        ...prev,
        lastMessage: `Error changing device: ${error}`,
        errorCount: prev.errorCount + 1,
      }));
    }
  };

  // Format dB values for display
  const formatDb = (value: number) => value.toFixed(1);

  // Get meter bar color based on level
  const getMeterColor = (level: number) => {
    if (level > -6) return 'bg-red-500';
    if (level > -12) return 'bg-yellow-500';
    return 'bg-green-500';
  };

  // Convert dB to percentage (0 = -60dB, 100 = 0dB)
  const dbToPercent = (db: number) => Math.max(0, Math.min(100, ((db + 60) / 60) * 100));

  return (
    <div className="p-4 bg-gray-900 text-white rounded-lg border border-gray-700">
      <h2 className="text-lg font-bold mb-4">IPC Communication Test</h2>

      {/* Connection Status */}
      <div className="mb-4 p-3 bg-gray-800 rounded">
        <div className="flex items-center justify-between">
          <span className="text-sm">Status:</span>
          <div className="flex items-center gap-2">
            <div
              className={`w-2 h-2 rounded-full ${
                status.connected ? 'bg-green-500' : 'bg-red-500'
              }`}
            ></div>
            <span className="text-xs">
              {status.connected ? 'Connected' : 'Disconnected'}
            </span>
          </div>
        </div>
        <div className="text-xs text-gray-400 mt-2">{status.lastMessage}</div>
        <div className="text-xs text-gray-500 mt-1">
          Meter Updates: {status.meterUpdates} | Errors: {status.errorCount}
        </div>
      </div>

      {/* Meter Visualization */}
      <div className="mb-4 p-3 bg-gray-800 rounded">
        <h3 className="text-sm font-semibold mb-3">Meter Data</h3>

        <div className="space-y-2">
          {/* Input Level */}
          <div>
            <div className="flex justify-between items-center mb-1">
              <span className="text-xs">Input:</span>
              <span className="text-xs font-mono">{formatDb(meterData.inputLevel)} dB</span>
            </div>
            <div className="h-2 bg-gray-700 rounded overflow-hidden">
              <div
                className={`h-full ${getMeterColor(meterData.inputLevel)} transition-all`}
                style={{ width: `${dbToPercent(meterData.inputLevel)}%` }}
              ></div>
            </div>
          </div>

          {/* Output Level */}
          <div>
            <div className="flex justify-between items-center mb-1">
              <span className="text-xs">Output:</span>
              <span className="text-xs font-mono">{formatDb(meterData.outputLevel)} dB</span>
            </div>
            <div className="h-2 bg-gray-700 rounded overflow-hidden">
              <div
                className={`h-full ${getMeterColor(meterData.outputLevel)} transition-all`}
                style={{ width: `${dbToPercent(meterData.outputLevel)}%` }}
              ></div>
            </div>
          </div>

          {/* Peak Levels */}
          <div className="text-xs text-gray-400 pt-2">
            Peaks: L {formatDb(meterData.peakLeft)}dB | R {formatDb(meterData.peakRight)}dB
          </div>
        </div>
      </div>

      {/* Device Selection */}
      <div className="mb-4 p-3 bg-gray-800 rounded">
        <h3 className="text-sm font-semibold mb-2">Audio Devices</h3>
        {devices.length > 0 ? (
          <select
            value={selectedDevice}
            onChange={(e) => handleDeviceChange(e.target.value)}
            className="w-full px-2 py-1 bg-gray-700 text-white text-sm rounded border border-gray-600"
          >
            {devices.map((device) => (
              <option key={device.id} value={device.id}>
                {device.name} {device.isInput ? '(In)' : '(Out)'}
              </option>
            ))}
          </select>
        ) : (
          <p className="text-xs text-gray-400">No devices loaded</p>
        )}
      </div>

      {/* Test Parameter Updates */}
      <div className="mb-4 p-3 bg-gray-800 rounded">
        <h3 className="text-sm font-semibold mb-3">Test Parameter Updates</h3>
        <div className="space-y-2">
          {[
            { effect: 'overdrive', param: 'drive', value: 0.5 },
            { effect: 'overdrive', param: 'tone', value: 0.7 },
            { effect: 'eq', param: 'bass', value: 0.6 },
            { effect: 'gain', param: 'level', value: 0.8 },
          ].map((item, idx) => (
            <button
              key={idx}
              onClick={() =>
                handleParameterUpdate(item.effect, item.param, item.value)
              }
              className="w-full px-3 py-1 bg-blue-600 hover:bg-blue-700 text-white text-xs rounded"
            >
              {item.effect}.{item.param} = {item.value}
            </button>
          ))}
        </div>
      </div>

      {/* Debug Info */}
      <div className="p-2 bg-gray-800 rounded text-xs text-gray-400 font-mono">
        <div>API Available: {audioEngine ? 'Yes' : 'No'}</div>
        <div>Meter Updates: {status.meterUpdates}</div>
        <div>Last Error Count: {status.errorCount}</div>
      </div>
    </div>
  );
};

export default IpcTestComponent;
