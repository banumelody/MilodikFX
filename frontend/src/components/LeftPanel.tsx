import React from 'react';
import { MeterBar, SceneGrid, LabeledValue } from './index';

export interface LeftPanelProps {
  selectedDevice?: string;
  devices?: string[];
  onDeviceChange?: (device: string) => void;
  inputLevel?: number;
  outputLevel?: number;
  tunerFrequency?: number;
  tunerNote?: string;
  scenes?: Array<{ id: number; name: string }>;
  selectedScene?: number;
  onSceneSelect?: (sceneId: number) => void;
  onTapTempo?: () => void;
}

export const LeftPanel: React.FC<LeftPanelProps> = ({
  selectedDevice,
  devices = [],
  onDeviceChange,
  inputLevel = -20,
  outputLevel = -18,
  tunerFrequency = 0,
  tunerNote = '',
  scenes = [],
  selectedScene,
  onSceneSelect,
  onTapTempo,
}) => {
  return (
    <div className="flex flex-col gap-4 p-4 rounded-lg bg-gray-900 border border-gray-700 min-w-64">
      {/* Device Selector */}
      {devices.length > 0 && (
        <div>
          <label className="text-sm font-semibold text-gray-300 mb-2 block">Device</label>
          <select
            value={selectedDevice || ''}
            onChange={(e) => onDeviceChange?.(e.target.value)}
            className="w-full px-3 py-2 bg-gray-800 border border-gray-600 rounded text-gray-300 text-sm"
          >
            <option value="">Select device...</option>
            {devices.map((device) => (
              <option key={device} value={device}>
                {device}
              </option>
            ))}
          </select>
        </div>
      )}

      {/* Metering */}
      <div className="flex flex-col gap-3">
        <h3 className="text-sm font-semibold text-gray-300">Metering</h3>
        <MeterBar value={inputLevel} min={-60} max={0} label="Input" color="green" />
        <MeterBar value={outputLevel} min={-60} max={0} label="Output" color="cyan" />
      </div>

      {/* Tuner */}
      {tunerFrequency !== undefined && (
        <div className="flex flex-col gap-2">
          <h3 className="text-sm font-semibold text-gray-300">Tuner</h3>
          <div className="flex gap-2">
            <LabeledValue
              label="Freq"
              value={tunerFrequency.toFixed(1)}
              units="Hz"
              size="sm"
            />
            <LabeledValue
              label="Note"
              value={tunerNote || '---'}
              size="sm"
            />
          </div>
        </div>
      )}

      {/* Scene Selector */}
      {scenes.length > 0 && (
        <div>
          <h3 className="text-sm font-semibold text-gray-300 mb-2">Scenes</h3>
          <SceneGrid
            scenes={scenes}
            selectedId={selectedScene}
            onClick={(id) => onSceneSelect?.(id)}
          />
        </div>
      )}

      {/* TAP Tempo */}
      {onTapTempo && (
        <button
          onClick={onTapTempo}
          className="w-full py-2 px-4 bg-amber-600 hover:bg-amber-700 text-white font-bold rounded transition-colors"
        >
          TAP TEMPO
        </button>
      )}
    </div>
  );
};
