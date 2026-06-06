import { useState, useEffect } from 'react';
import { useAudioEngine } from '../hooks';
import { Button, Input, Card } from './index';

interface PresetBarProps {
  onPresetLoaded?: () => void;
  onPresetSaved?: () => void;
}

export default function PresetBar({ onPresetLoaded, onPresetSaved }: PresetBarProps) {
  const { savePreset, loadPreset, deletePreset, listPresets } = useAudioEngine();
  const [currentPresetName, setCurrentPresetName] = useState<string>('Untitled');
  const [presets, setPresets] = useState<Array<{ id: string; name: string }>>([]);
  const [showSaveModal, setShowSaveModal] = useState(false);
  const [showLoadModal, setShowLoadModal] = useState(false);
  const [newPresetName, setNewPresetName] = useState('');
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    loadPresetsAsync();
  }, []);

  const loadPresetsAsync = async () => {
    try {
      const presetList = await listPresets();
      setPresets((presetList || []).map((p: any, idx: number) => ({ id: `${idx}`, name: p.name || 'Preset' })));
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load presets');
    }
  };

  const handleSavePreset = async () => {
    if (!newPresetName.trim()) {
      setError('Please enter a preset name');
      return;
    }
    try {
      await savePreset(newPresetName);
      setCurrentPresetName(newPresetName);
      setNewPresetName('');
      setShowSaveModal(false);
      setError(null);
      loadPresetsAsync();
      onPresetSaved?.();
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to save preset');
    }
  };

  const handleLoadPreset = async (presetId: string) => {
    try {
      await loadPreset(presetId);
      const preset = presets.find((p) => p.id === presetId);
      if (preset) setCurrentPresetName(preset.name);
      setShowLoadModal(false);
      setError(null);
      onPresetLoaded?.();
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load preset');
    }
  };

  const handleDeletePreset = async (presetId: string) => {
    try {
      await deletePreset(presetId);
      setPresets(presets.filter((p) => p.id !== presetId));
      loadPresetsAsync();
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to delete preset');
    }
  };

  return (
    <Card className="bg-gradient-to-r from-blue-900 to-blue-800 rounded-lg shadow-lg">
      <div className="flex items-center justify-between gap-4 p-4">
        <div className="flex items-center gap-4 flex-1">
          <div>
            <p className="text-sm text-blue-100">Current Preset</p>
            <p className="text-lg font-semibold text-white">{currentPresetName}</p>
          </div>
        </div>

        <div className="flex gap-2">
          <Button
            variant="primary"
            size="md"
            onClick={() => setShowSaveModal(true)}
            title="Save current settings as preset"
          >
            💾 Save
          </Button>

          <Button
            variant="secondary"
            size="md"
            onClick={() => setShowLoadModal(true)}
            title="Load a saved preset"
          >
            📂 Load
          </Button>
        </div>

        {error && <div className="text-red-400 text-sm">{error}</div>}
      </div>

      {/* Save Modal */}
      {showSaveModal && (
        <div className="fixed inset-0 bg-black/50 flex items-center justify-center z-50">
          <div className="bg-gray-900 rounded-lg p-6 max-w-sm w-full mx-4">
            <h3 className="text-lg font-semibold text-white mb-4">Save Preset As</h3>
            <Input
              value={newPresetName}
              onChange={(e) => setNewPresetName(e.currentTarget.value)}
              placeholder="Enter preset name"
              autoFocus
            />
            <div className="flex gap-2 mt-4">
              <Button
                variant="primary"
                onClick={handleSavePreset}
              >
                Save
              </Button>
              <Button
                variant="ghost"
                onClick={() => {
                  setShowSaveModal(false);
                  setNewPresetName('');
                }}
              >
                Cancel
              </Button>
            </div>
          </div>
        </div>
      )}

      {/* Load Modal */}
      {showLoadModal && (
        <div className="fixed inset-0 bg-black/50 flex items-center justify-center z-50">
          <div className="bg-gray-900 rounded-lg p-6 max-w-md w-full mx-4 max-h-96 overflow-y-auto">
            <h3 className="text-lg font-semibold text-white mb-4">Load Preset</h3>
            {presets.length === 0 ? (
              <p className="text-gray-400">No presets saved yet</p>
            ) : (
              <div className="space-y-2">
                {presets.map((preset) => (
                  <div key={preset.id} className="flex items-center justify-between bg-gray-800 p-2 rounded">
                    <span className="text-white">{preset.name}</span>
                    <div className="flex gap-2">
                      <Button
                        variant="primary"
                        size="sm"
                        onClick={() => handleLoadPreset(preset.id)}
                      >
                        Load
                      </Button>
                      <Button
                        variant="danger"
                        size="sm"
                        onClick={() => handleDeletePreset(preset.id)}
                      >
                        Delete
                      </Button>
                    </div>
                  </div>
                ))}
              </div>
            )}
            <Button
              variant="ghost"
              onClick={() => setShowLoadModal(false)}
              className="w-full mt-4"
            >
              Close
            </Button>
          </div>
        </div>
      )}
    </Card>
  );
}
