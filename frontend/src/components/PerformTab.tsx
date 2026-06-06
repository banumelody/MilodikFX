import React, { useState } from 'react';
import { SignalChainCanvas, SceneGrid, ExpressionAssignment, AddEffectModal, Effect } from './index';

export interface PerformTabProps {
  effects?: Effect[];
  scenes?: Array<{ id: number; name: string }>;
  onAddEffect?: (effectType: string) => void;
  onRemoveEffect?: (effectId: string) => void;
  onReorderEffects?: (newOrder: string[]) => void;
  onSceneSelect?: (sceneId: number) => void;
}

export const PerformTab: React.FC<PerformTabProps> = ({
  effects = [],
  scenes = [
    { id: 1, name: 'Clean' },
    { id: 2, name: 'Crunch' },
    { id: 3, name: 'Lead' },
    { id: 4, name: 'Custom' },
  ],
  onAddEffect,
  onRemoveEffect,
  onReorderEffects,
  onSceneSelect,
}) => {
  const [selectedScene, setSelectedScene] = useState<number>(1);
  const [showAddEffectModal, setShowAddEffectModal] = useState(false);

  const handleSceneSelect = (sceneId: number) => {
    setSelectedScene(sceneId);
    onSceneSelect?.(sceneId);
  };

  const handleAddEffectClick = () => {
    setShowAddEffectModal(true);
  };

  const handleSelectEffectType = (effectType: string) => {
    onAddEffect?.(effectType);
  };

  return (
    <div className="flex flex-col gap-6 p-6">
      {/* Signal Chain Section */}
      <div className="flex flex-col gap-4">
        <h2 className="text-xl font-bold text-white">Signal Chain</h2>
        <SignalChainCanvas
          effects={effects}
          onAdd={handleAddEffectClick}
          onRemove={onRemoveEffect || (() => {})}
          onReorder={onReorderEffects || (() => {})}
        />
      </div>

      {/* Scenes Section */}
      <div className="flex flex-col gap-4">
        <h2 className="text-xl font-bold text-white">Scenes</h2>
        <SceneGrid
          scenes={scenes}
          selectedId={selectedScene}
          onClick={handleSceneSelect}
        />
      </div>

      {/* Expression Pedal Section */}
      <div className="flex flex-col gap-4">
        <h2 className="text-xl font-bold text-white">Expression Pedal</h2>
        <div className="grid grid-cols-3 gap-4">
          <ExpressionAssignment
            expNum={1}
            onChange={(paramId) => {
              console.log(`EXP 1 assigned to: ${paramId}`);
            }}
            availableParams={[
              { value: 'gain', label: 'Gain Level' },
              { value: 'drive', label: 'Drive Amount' },
              { value: 'mix', label: 'Effect Mix' },
              { value: 'none', label: 'None' },
            ]}
          />
          <ExpressionAssignment
            expNum={2}
            onChange={(paramId) => {
              console.log(`EXP 2 assigned to: ${paramId}`);
            }}
            availableParams={[
              { value: 'gain', label: 'Gain Level' },
              { value: 'drive', label: 'Drive Amount' },
              { value: 'mix', label: 'Effect Mix' },
              { value: 'none', label: 'None' },
            ]}
          />
          <ExpressionAssignment
            expNum={3}
            onChange={(paramId) => {
              console.log(`EXP 3 assigned to: ${paramId}`);
            }}
            availableParams={[
              { value: 'gain', label: 'Gain Level' },
              { value: 'drive', label: 'Drive Amount' },
              { value: 'mix', label: 'Effect Mix' },
              { value: 'none', label: 'None' },
            ]}
          />
        </div>
      </div>

      {/* Add Effect Modal */}
      <AddEffectModal
        isOpen={showAddEffectModal}
        onClose={() => setShowAddEffectModal(false)}
        onSelectEffect={handleSelectEffectType}
      />
    </div>
  );
};
