import React from 'react';
import { MasterVolumeKnob, PerformanceGraph, StatusIndicator } from './index';

export interface RightPanelProps {
  masterVolume?: number;
  isMuted?: boolean;
  onVolumeChange?: (value: number) => void;
  onMute?: (muted: boolean) => void;
  outputLevel?: number;
  cpuHistory?: number[];
  cpuLoad?: number;
  sampleRate?: number;
  bufferSize?: number;
  notes?: string;
  onNotesChange?: (notes: string) => void;
  isAudioRunning?: boolean;
}

export const RightPanel: React.FC<RightPanelProps> = ({
  masterVolume = 75,
  isMuted = false,
  onVolumeChange,
  onMute,
  outputLevel = -18,
  cpuHistory = [],
  cpuLoad = 5,
  sampleRate = 44100,
  bufferSize = 256,
  notes = '',
  onNotesChange,
  isAudioRunning = false,
}) => {
  return (
    <div className="flex flex-col gap-4 p-4 rounded-lg bg-gray-900 border border-gray-700 min-w-72 max-h-screen overflow-y-auto">
      {/* Master Volume */}
      <div className="flex justify-center">
        <MasterVolumeKnob
          value={masterVolume}
          isMuted={isMuted}
          onChange={(val) => onVolumeChange?.(val)}
          onMute={(muted) => onMute?.(muted)}
          level={outputLevel}
        />
      </div>

      {/* CPU Graph */}
      {cpuHistory.length > 0 && (
        <PerformanceGraph
          cpuHistory={cpuHistory}
          maxValue={100}
          showLegend
        />
      )}

      {/* Status Indicators */}
      <div className="flex flex-col gap-2">
        <h3 className="text-sm font-semibold text-gray-300">Status</h3>
        <StatusIndicator
          status={isAudioRunning ? 'active' : 'idle'}
          label={isAudioRunning ? 'Audio Running' : 'Audio Idle'}
        />
        <StatusIndicator
          status={cpuLoad > 90 ? 'error' : cpuLoad > 80 ? 'warning' : 'idle'}
          label={`CPU ${cpuLoad.toFixed(1)}%`}
        />
      </div>

      {/* Audio Config */}
      <div className="text-xs text-gray-400 space-y-1">
        <div>Sample Rate: {sampleRate} Hz</div>
        <div>Buffer: {bufferSize} samples</div>
      </div>

      {/* Notes */}
      {onNotesChange && (
        <div className="flex flex-col gap-2">
          <label className="text-sm font-semibold text-gray-300">Notes</label>
          <textarea
            value={notes}
            onChange={(e) => onNotesChange(e.target.value)}
            placeholder="Add notes about this preset..."
            className="w-full px-3 py-2 bg-gray-800 border border-gray-600 rounded text-gray-300 text-sm resize-none"
            rows={3}
          />
        </div>
      )}
    </div>
  );
};
