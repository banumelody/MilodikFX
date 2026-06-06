import React from 'react';
import { SceneButton } from './SceneButton';

export interface Scene {
  id: number;
  name: string;
  effectColors?: string[];
}

export interface SceneGridProps {
  scenes: Scene[];
  selectedId?: number;
  onClick: (sceneId: number) => void;
}

export const SceneGrid: React.FC<SceneGridProps> = ({
  scenes,
  selectedId,
  onClick,
}) => {
  return (
    <div className="grid grid-cols-2 gap-3">
      {scenes.map((scene) => (
        <SceneButton
          key={scene.id}
          sceneNum={scene.id}
          name={scene.name}
          isActive={scene.id === selectedId}
          onClick={() => onClick(scene.id)}
          effectColors={scene.effectColors}
        />
      ))}
    </div>
  );
};
