import React, { useState } from 'react';
import { SignalChainBlock } from './index';
import { Button } from './Button';

export interface Effect {
  id: string;
  name: string;
  type: string;
  position: number;
}

export interface SignalChainCanvasProps {
  effects: Effect[];
  onAdd: () => void;
  onRemove: (effectId: string) => void;
  onReorder: (newOrder: string[]) => void;
  onClear?: () => void;
}

export const SignalChainCanvas: React.FC<SignalChainCanvasProps> = ({
  effects,
  onAdd,
  onRemove,
  onReorder,
  onClear,
}) => {
  const [draggedId, setDraggedId] = useState<string | null>(null);

  const handleDragStart = (e: React.DragEvent<HTMLDivElement>, id: string) => {
    setDraggedId(id);
    e.dataTransfer.effectAllowed = 'move';
  };

  const handleDragOver = (e: React.DragEvent<HTMLDivElement>) => {
    e.preventDefault();
    e.dataTransfer.dropEffect = 'move';
  };

  const handleDrop = (e: React.DragEvent<HTMLDivElement>, targetId: string) => {
    e.preventDefault();
    if (draggedId && draggedId !== targetId) {
      const draggedIndex = effects.findIndex((e) => e.id === draggedId);
      const targetIndex = effects.findIndex((e) => e.id === targetId);

      if (draggedIndex >= 0 && targetIndex >= 0) {
        const newEffects = [...effects];
        newEffects.splice(draggedIndex, 1);
        newEffects.splice(targetIndex, 0, effects[draggedIndex]);
        onReorder(newEffects.map((e) => e.id));
      }
    }
    setDraggedId(null);
  };

  return (
    <div className="flex flex-col gap-4 p-4 rounded-lg bg-gray-900 border border-gray-700">
      <div className="flex items-center justify-between">
        <h3 className="text-lg font-bold text-white">Signal Chain</h3>
        <div className="flex gap-2">
          <Button variant="primary" size="sm" onClick={onAdd}>
            + Add Effect
          </Button>
          {onClear && (
            <Button variant="danger" size="sm" onClick={onClear}>
              Clear
            </Button>
          )}
        </div>
      </div>

      <div className="relative min-h-24 flex gap-2 items-center overflow-x-auto p-2 rounded bg-gray-800 border border-gray-700">
        {effects.length === 0 ? (
          <div className="text-gray-500 text-sm">No effects. Click "Add Effect" to get started.</div>
        ) : (
          effects.map((effect, index) => (
            <div
              key={effect.id}
              onDragOver={handleDragOver}
              onDrop={(e) => handleDrop(e, effect.id)}
              className="flex items-center gap-2"
            >
              {index > 0 && (
                <div className="text-gray-500 text-xl">→</div>
              )}
              <SignalChainBlock
                effectId={effect.id}
                name={effect.name}
                color={effect.type}
                isDragging={draggedId === effect.id}
                onDragStart={(e) => handleDragStart(e, effect.id)}
                onRemove={() => onRemove(effect.id)}
              />
            </div>
          ))
        )}
      </div>
    </div>
  );
};
