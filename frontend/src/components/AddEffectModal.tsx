import React from 'react';
import { Modal } from './index';

export interface AddEffectModalProps {
  isOpen: boolean;
  onClose: () => void;
  onSelectEffect: (effectType: string) => void;
}

interface EffectOption {
  id: string;
  name: string;
  description: string;
  color: string;
}

const effectOptions: EffectOption[] = [
  {
    id: 'GAIN',
    name: 'Gain',
    description: 'Adjustable input/output level',
    color: 'bg-green-900',
  },
  {
    id: 'OVERDRIVE',
    name: 'Overdrive',
    description: 'Warm, musical distortion',
    color: 'bg-orange-900',
  },
  {
    id: 'EQ',
    name: 'Equalizer',
    description: '3-band parametric EQ',
    color: 'bg-cyan-900',
  },
  {
    id: 'COMP',
    name: 'Compressor',
    description: 'Dynamic range control',
    color: 'bg-blue-900',
  },
  {
    id: 'NOISE_GATE',
    name: 'Noise Gate',
    description: 'Silence quiet signals',
    color: 'bg-lime-900',
  },
  {
    id: 'DELAY',
    name: 'Delay',
    description: 'Time-based echo effect',
    color: 'bg-purple-900',
  },
  {
    id: 'REVERB',
    name: 'Reverb',
    description: 'Spatial ambience effect',
    color: 'bg-indigo-900',
  },
];

export const AddEffectModal: React.FC<AddEffectModalProps> = ({
  isOpen,
  onClose,
  onSelectEffect,
}) => {
  const handleSelectEffect = (effectId: string) => {
    onSelectEffect(effectId);
    onClose();
  };

  return (
    <Modal isOpen={isOpen} title="Add Effect" onClose={onClose} size="md">
      <div className="flex flex-col gap-3">
        {effectOptions.map((effect) => (
          <button
            key={effect.id}
            onClick={() => handleSelectEffect(effect.id)}
            className={`
              flex flex-col gap-2 p-4 rounded-lg border border-gray-600
              hover:border-gray-500 transition-all
              ${effect.color} bg-opacity-20
              hover:bg-opacity-30 cursor-pointer
            `}
          >
            <h3 className="font-bold text-white text-left">{effect.name}</h3>
            <p className="text-sm text-gray-300 text-left">{effect.description}</p>
          </button>
        ))}
      </div>
    </Modal>
  );
};
