import { useState } from 'react';
import { Card, Button } from './index';

interface Effect {
  id: string;
  type: string;
  name: string;
  enabled: boolean;
  parameters: Record<string, { value: number; min: number; max: number; unit?: string }>;
}

interface EffectCardsGridProps {
  onAddEffect?: (type: string) => void;
  onRemoveEffect?: (effectId: string) => void;
  onParameterChange?: (effectId: string, paramName: string, value: number) => void;
  effects?: Effect[];
}

export default function EffectCardsGrid({
  onAddEffect,
  onRemoveEffect,
  onParameterChange,
  effects = [],
}: EffectCardsGridProps) {
  const [showAddModal, setShowAddModal] = useState(false);
  const [effectTypes] = useState([
    { id: 'gain', name: 'Gain' },
    { id: 'overdrive', name: 'Overdrive' },
    { id: 'eq', name: 'EQ' },
    { id: 'compressor', name: 'Compressor' },
    { id: 'reverb', name: 'Reverb' },
  ]);

  const handleAddEffect = (typeId: string) => {
    onAddEffect?.(typeId);
    setShowAddModal(false);
  };

  return (
    <div className="space-y-4">
      <h3 className="text-sm font-semibold text-gray-300 uppercase tracking-wide">
        🎛️ Signal Chain
      </h3>

      {effects.length === 0 ? (
        <Card className="bg-gray-900 rounded-lg border-2 border-dashed border-gray-700 p-8 text-center">
          <p className="text-gray-500 mb-4">No effects in chain</p>
          <Button variant="primary" onClick={() => setShowAddModal(true)}>
            + Add Effect
          </Button>
        </Card>
      ) : (
        <>
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
            {effects.map((effect) => (
              <Card
                key={effect.id}
                title={effect.name}
                className="bg-gradient-to-br from-gray-800 to-gray-900 rounded-lg"
                onRemove={() => onRemoveEffect?.(effect.id)}
              >
                <div className="p-4 space-y-4">
                  {/* Enabled Toggle */}
                  <div className="flex items-center justify-between">
                    <span className="text-sm text-gray-400">Enabled</span>
                    <button
                      className={`w-10 h-6 rounded-full transition-colors ${
                        effect.enabled ? 'bg-green-600' : 'bg-gray-600'
                      }`}
                      onClick={() => {
                        // Toggle effect
                      }}
                    >
                      <div
                        className={`w-5 h-5 bg-white rounded-full transition-transform ${
                          effect.enabled ? 'translate-x-4' : 'translate-x-0.5'
                        }`}
                      />
                    </button>
                  </div>

                  {/* Parameters */}
                  <div className="space-y-3">
                    {Object.entries(effect.parameters).map(([paramName, paramData]) => (
                      <div key={paramName}>
                        <div className="flex justify-between items-center mb-2">
                          <label className="text-xs font-medium text-gray-400">
                            {paramName}
                          </label>
                          <span className="text-xs text-gray-500 font-mono">
                            {paramData.value.toFixed(1)}{paramData.unit || ''}
                          </span>
                        </div>
                        <input
                          type="range"
                          min={paramData.min}
                          max={paramData.max}
                          value={paramData.value}
                          onChange={(e) => {
                            const value = parseFloat(e.target.value);
                            onParameterChange?.(effect.id, paramName, value);
                          }}
                          className="w-full h-2 bg-gray-700 rounded-lg appearance-none cursor-pointer"
                        />
                      </div>
                    ))}
                  </div>
                </div>
              </Card>
            ))}
          </div>

          <Button
            variant="secondary"
            size="md"
            onClick={() => setShowAddModal(true)}
            className="w-full mt-4"
          >
            + Add Effect
          </Button>
        </>
      )}

      {/* Add Effect Modal */}
      {showAddModal && (
        <div className="fixed inset-0 bg-black/50 flex items-center justify-center z-50">
          <div className="bg-gray-900 rounded-lg p-6 max-w-sm w-full mx-4">
            <h3 className="text-lg font-semibold text-white mb-4">Add Effect</h3>
            <div className="space-y-2 max-h-64 overflow-y-auto">
              {effectTypes.map((type) => (
                <button
                  key={type.id}
                  onClick={() => handleAddEffect(type.id)}
                  className="w-full text-left p-3 bg-gray-800 hover:bg-gray-700 rounded transition-colors"
                >
                  <p className="text-white font-medium">{type.name}</p>
                </button>
              ))}
            </div>
            <Button
              variant="ghost"
              onClick={() => setShowAddModal(false)}
              className="w-full mt-4"
            >
              Cancel
            </Button>
          </div>
        </div>
      )}
    </div>
  );
}
