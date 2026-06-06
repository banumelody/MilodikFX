import React from 'react';
import { Knob, MeterBar } from './index';

export interface MasterVolumeKnobProps {
  value: number;
  isMuted: boolean;
  onChange: (value: number) => void;
  onMute: (muted: boolean) => void;
  level?: number;
}

export const MasterVolumeKnob: React.FC<MasterVolumeKnobProps> = ({
  value,
  isMuted,
  onChange,
  onMute,
  level = 0,
}) => {
  return (
    <div className="flex flex-col items-center gap-4">
      <div className="relative">
        <Knob
          value={isMuted ? 0 : value}
          min={0}
          max={100}
          onChange={onChange}
          label="Master"
          size="lg"
          disabled={isMuted}
        />
        <button
          onClick={() => onMute(!isMuted)}
          className={`
            absolute bottom-0 right-0 w-8 h-8 rounded-full
            transition-colors duration-200
            ${isMuted ? 'bg-red-600 text-white' : 'bg-gray-600 text-white'}
            hover:opacity-80
          `}
          title={isMuted ? 'Unmute' : 'Mute'}
        >
          {isMuted ? '🔇' : '🔊'}
        </button>
      </div>
      <MeterBar
        value={level}
        max={100}
        color="cyan"
        label="Output"
        showPeak
      />
    </div>
  );
};
