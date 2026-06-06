import React from 'react';
import { Knob, ToggleSwitch, LabeledValue } from './index';

export interface EffectParameter {
  id: string;
  name: string;
  value: number;
  min: number;
  max: number;
  unit?: string;
}

export interface EffectCardProps {
  type: 'GAIN' | 'OVERDRIVE' | 'EQ' | 'NOISE_GATE' | 'COMP' | 'DELAY' | 'REVERB';
  title: string;
  enabled: boolean;
  parameters: EffectParameter[];
  onToggle: (enabled: boolean) => void;
  onParameterChange: (paramId: string, value: number) => void;
  onRemove?: () => void;
}

const colorMap: Record<string, string> = {
  GAIN: 'border-green-500 bg-green-500/10',
  OVERDRIVE: 'border-orange-500 bg-orange-500/10',
  EQ: 'border-cyan-500 bg-cyan-500/10',
  NOISE_GATE: 'border-lime-500 bg-lime-500/10',
  COMP: 'border-blue-500 bg-blue-500/10',
  DELAY: 'border-purple-500 bg-purple-500/10',
  REVERB: 'border-indigo-500 bg-indigo-500/10',
};

export const EffectCard: React.FC<EffectCardProps> = ({
  type,
  title,
  enabled,
  parameters,
  onToggle,
  onParameterChange,
  onRemove,
}) => {
  const colorClass = colorMap[type] || colorMap['GAIN'];

  return (
    <div
      className={`
        rounded-lg border-2 p-4 transition-all duration-200
        ${colorClass}
        ${enabled ? 'opacity-100' : 'opacity-50'}
      `}
    >
      {/* Header */}
      <div className="flex items-center justify-between mb-4">
        <div>
          <h3 className="text-lg font-bold text-white">{title}</h3>
          <p className="text-xs text-gray-400">{type}</p>
        </div>
        <div className="flex items-center gap-2">
          <ToggleSwitch
            checked={enabled}
            onChange={onToggle}
            size="md"
          />
          {onRemove && (
            <button
              onClick={onRemove}
              className="text-red-400 hover:text-red-500 transition-colors"
              title="Remove effect"
            >
              ✕
            </button>
          )}
        </div>
      </div>

      {/* Parameters Grid */}
      <div className="grid grid-cols-2 gap-4">
        {parameters.map((param) => (
          <div key={param.id} className="flex flex-col items-center gap-2">
            <Knob
              value={param.value}
              min={param.min}
              max={param.max}
              onChange={(value) => onParameterChange(param.id, value)}
              label={param.name}
              size="sm"
              disabled={!enabled}
            />
            <LabeledValue
              label={param.name}
              value={param.value.toFixed(1)}
              units={param.unit}
              size="sm"
            />
          </div>
        ))}
      </div>
    </div>
  );
};
