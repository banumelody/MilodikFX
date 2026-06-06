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

  return {
    isConnected,
    error,
    setParameter,
  };
};
