import { useEffect, useState, useCallback } from 'react';
import { audioEngine, PresetState } from '../services/audioEngine';
import { eventDispatcher } from '../services/eventDispatcher';

export const usePresets = () => {
  const [presets, setPresets] = useState<PresetState[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const loadPresets = async () => {
      setLoading(true);
      try {
        const loaded = await audioEngine.getPresets();
        setPresets(loaded);
      } catch (err) {
        setError(err instanceof Error ? err.message : 'Failed to load presets');
      } finally {
        setLoading(false);
      }
    };

    loadPresets();

    const handlePresetSaved = (preset: PresetState) => {
      setPresets((prev) => [...prev, preset]);
    };

    const handlePresetDeleted = ({ id }: { id: string }) => {
      setPresets((prev) => prev.filter((p) => p.name !== id));
    };

    eventDispatcher.on('presetSaved', handlePresetSaved);
    eventDispatcher.on('presetDeleted', handlePresetDeleted);

    return () => {
      eventDispatcher.off('presetSaved', handlePresetSaved);
      eventDispatcher.off('presetDeleted', handlePresetDeleted);
    };
  }, []);

  const savePreset = useCallback(async (preset: PresetState) => {
    try {
      await audioEngine.savePreset(preset);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to save preset');
      throw err;
    }
  }, []);

  const loadPreset = useCallback(async (presetId: string) => {
    try {
      await audioEngine.loadPreset(presetId);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load preset');
      throw err;
    }
  }, []);

  const deletePreset = useCallback(async (presetId: string) => {
    try {
      await audioEngine.deletePreset(presetId);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to delete preset');
      throw err;
    }
  }, []);

  return {
    presets,
    loading,
    error,
    savePreset,
    loadPreset,
    deletePreset,
  };
};
