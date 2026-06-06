import { useEffect, useState, useCallback } from 'react';
import { audioEngine } from '../services/audioEngine';

export const useAudioEngine = () => {
  const [isConnected, setIsConnected] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const initializeEngine = async () => {
      try {
        // In a real app, this would initialize the connection
        setIsConnected(true);
      } catch (err) {
        setError(err instanceof Error ? err.message : 'Failed to connect to audio engine');
        setIsConnected(false);
      }
    };

    initializeEngine();
  }, []);

  const setParameter = useCallback(async (processor: string, param: string, value: number) => {
    try {
      await audioEngine.setParameter(processor, param, value);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to set parameter');
    }
  }, []);

  const savePreset = useCallback(async (name: string) => {
    try {
      await audioEngine.savePreset({ name, processors: {} });
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to save preset');
    }
  }, []);

  const loadPreset = useCallback(async (presetId: string) => {
    try {
      await audioEngine.loadPreset(presetId);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load preset');
    }
  }, []);

  const deletePreset = useCallback(async (presetId: string) => {
    try {
      await audioEngine.deletePreset(presetId);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to delete preset');
    }
  }, []);

  const listPresets = useCallback(async () => {
    try {
      return await audioEngine.getPresets();
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to list presets');
      return [];
    }
  }, []);

  return {
    isConnected,
    error,
    setParameter,
    savePreset,
    loadPreset,
    deletePreset,
    listPresets,
  };
};
