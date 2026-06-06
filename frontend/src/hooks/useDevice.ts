import { useEffect, useState, useCallback } from 'react';
import { audioEngine } from '../services/audioEngine';
import { eventDispatcher } from '../services/eventDispatcher';

export interface AudioDevices {
  inputs: string[];
  outputs: string[];
  selectedInput?: string;
  selectedOutput?: string;
}

export const useDevice = () => {
  const [devices, setDevices] = useState<AudioDevices>({
    inputs: [],
    outputs: [],
  });
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const loadDevices = async () => {
      setLoading(true);
      try {
        const deviceList = await audioEngine.getDevices();
        setDevices(deviceList);
      } catch (err) {
        setError(err instanceof Error ? err.message : 'Failed to load devices');
      } finally {
        setLoading(false);
      }
    };

    loadDevices();

    const handleDeviceChanged = (data: { type: 'input' | 'output'; deviceId: string }) => {
      setDevices((prev) => ({
        ...prev,
        [data.type === 'input' ? 'selectedInput' : 'selectedOutput']: data.deviceId,
      }));
    };

    eventDispatcher.on('deviceChanged', handleDeviceChanged);

    return () => {
      eventDispatcher.off('deviceChanged', handleDeviceChanged);
    };
  }, []);

  const setInputDevice = useCallback(async (deviceId: string) => {
    try {
      await audioEngine.setInputDevice(deviceId);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to set input device');
      throw err;
    }
  }, []);

  const setOutputDevice = useCallback(async (deviceId: string) => {
    try {
      await audioEngine.setOutputDevice(deviceId);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to set output device');
      throw err;
    }
  }, []);

  return {
    devices,
    loading,
    error,
    setInputDevice,
    setOutputDevice,
  };
};
