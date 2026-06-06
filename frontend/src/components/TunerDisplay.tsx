import React from 'react';

export interface TunerDisplayProps {
  frequency: number;
  note: string;
  cents: number;
}

export const TunerDisplay: React.FC<TunerDisplayProps> = ({
  frequency,
  note,
  cents,
}) => {
  const isInTune = Math.abs(cents) < 5;
  const noteColor = isInTune ? 'text-green-400' : cents > 0 ? 'text-amber-400' : 'text-blue-400';
  const centsDirection = cents > 0 ? '↑' : cents < 0 ? '↓' : '●';

  return (
    <div className="flex flex-col items-center justify-center gap-4 p-6 rounded-lg bg-gray-900 border-2 border-cyan-500">
      <div className="text-5xl font-bold text-cyan-400">
        {frequency.toFixed(1)} Hz
      </div>
      <div className={`text-6xl font-bold ${noteColor}`}>
        {note || '---'}
      </div>
      <div className="flex items-center gap-2">
        <span className={`text-3xl ${noteColor}`}>
          {centsDirection}
        </span>
        <span className={`text-2xl font-mono ${noteColor}`}>
          {Math.abs(cents).toFixed(1)}¢
        </span>
      </div>
      <div className="w-full h-1 bg-gray-700 rounded overflow-hidden">
        <div
          className={`h-full transition-all duration-100 ${
            isInTune ? 'bg-green-500' : 'bg-amber-500'
          }`}
          style={{
            width: `${Math.min(100, 50 + (cents * 2))}%`,
          }}
        />
      </div>
    </div>
  );
};
