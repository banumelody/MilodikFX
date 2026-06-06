import React from 'react';

export interface SignalChainBlockProps {
  effectId: string;
  name: string;
  color: string;
  isDragging?: boolean;
  onRemove?: () => void;
  onDragStart?: (e: React.DragEvent<HTMLDivElement>) => void;
}

const colorMap: Record<string, string> = {
  GAIN: 'bg-green-600',
  OVERDRIVE: 'bg-orange-600',
  EQ: 'bg-cyan-600',
  NOISE_GATE: 'bg-lime-600',
  COMP: 'bg-blue-600',
  DELAY: 'bg-purple-600',
  REVERB: 'bg-indigo-600',
};

export const SignalChainBlock: React.FC<SignalChainBlockProps> = ({
  effectId,
  name,
  color,
  isDragging,
  onRemove,
  onDragStart,
}) => {
  const bgColor = colorMap[color] || colorMap['GAIN'];

  return (
    <div
      draggable
      onDragStart={onDragStart}
      className={`
        ${bgColor}
        rounded-lg p-4 cursor-move
        transition-all duration-200
        ${isDragging ? 'opacity-50 scale-95' : 'hover:shadow-lg'}
        border-2 border-opacity-50 border-white
        min-w-[140px]
      `}
    >
      <div className="flex items-center justify-between gap-2">
        <div className="flex-1">
          <h4 className="text-sm font-bold text-white">{name}</h4>
          <p className="text-xs text-gray-200">{effectId.slice(0, 8)}</p>
        </div>
        {onRemove && (
          <button
            onClick={onRemove}
            className="text-white hover:text-red-300 transition-colors"
            title="Remove effect"
          >
            ✕
          </button>
        )}
      </div>
    </div>
  );
};
