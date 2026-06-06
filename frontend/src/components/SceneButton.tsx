import React from 'react';

export interface SceneButtonProps {
  sceneNum: number;
  name: string;
  isActive: boolean;
  onClick: () => void;
  effectColors?: string[];
}

export const SceneButton: React.FC<SceneButtonProps> = ({
  sceneNum,
  name,
  isActive,
  onClick,
  effectColors = [],
}) => {
  return (
    <button
      onClick={onClick}
      className={`
        flex flex-col items-center justify-center gap-2 p-4 rounded-lg
        transition-all duration-200 border-2
        ${
          isActive
            ? 'border-cyan-500 bg-cyan-500/20 shadow-lg shadow-cyan-500/50'
            : 'border-gray-600 bg-gray-800 hover:border-gray-500'
        }
      `}
    >
      <div className="text-sm font-bold text-gray-400">{`Scene ${sceneNum}`}</div>
      <div className="text-lg font-semibold text-white">{name}</div>
      {effectColors.length > 0 && (
        <div className="flex gap-1">
          {effectColors.slice(0, 4).map((color, idx) => (
            <div
              key={idx}
              className="w-2 h-2 rounded-full"
              style={{ backgroundColor: color }}
            />
          ))}
          {effectColors.length > 4 && (
            <span className="text-xs text-gray-500">+{effectColors.length - 4}</span>
          )}
        </div>
      )}
    </button>
  );
};
