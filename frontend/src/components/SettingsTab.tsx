import React from 'react';
import { Dropdown, DropdownOption, ToggleSwitch } from './index';

export interface SettingsTabProps {
  devices?: DropdownOption[];
  selectedDevice?: string;
  onDeviceChange?: (device: string) => void;
  sampleRates?: DropdownOption[];
  selectedSampleRate?: string;
  onSampleRateChange?: (rate: string) => void;
  bufferSizes?: DropdownOption[];
  selectedBufferSize?: string;
  onBufferSizeChange?: (size: string) => void;
  theme?: 'dark' | 'light';
  onThemeChange?: (theme: 'dark' | 'light') => void;
  scaling?: number;
  onScalingChange?: (scaling: number) => void;
  midiDevices?: DropdownOption[];
  selectedMidiDevice?: string;
  onMidiDeviceChange?: (device: string) => void;
}

export const SettingsTab: React.FC<SettingsTabProps> = ({
  devices = [],
  selectedDevice,
  onDeviceChange,
  sampleRates = [
    { value: '44100', label: '44.1 kHz' },
    { value: '48000', label: '48 kHz' },
    { value: '96000', label: '96 kHz' },
  ],
  selectedSampleRate,
  onSampleRateChange,
  bufferSizes = [
    { value: '128', label: '128' },
    { value: '256', label: '256' },
    { value: '512', label: '512' },
  ],
  selectedBufferSize,
  onBufferSizeChange,
  theme = 'dark',
  onThemeChange,
  scaling = 100,
  onScalingChange,
  midiDevices = [],
  selectedMidiDevice,
  onMidiDeviceChange,
}) => {
  return (
    <div className="p-6 space-y-6">
      {/* Audio Settings */}
      <div className="rounded-lg bg-gray-800 border border-gray-700 p-4">
        <h3 className="text-lg font-bold text-white mb-4">Audio Settings</h3>
        <div className="space-y-4">
          {devices.length > 0 && (
            <div>
              <label className="block text-sm font-medium text-gray-300 mb-2">Device</label>
              <Dropdown
                options={devices}
                value={selectedDevice || ''}
                onChange={(value) => onDeviceChange?.(String(value))}
                placeholder="Select audio device..."
              />
            </div>
          )}
          <div>
            <label className="block text-sm font-medium text-gray-300 mb-2">Sample Rate</label>
            <Dropdown
              options={sampleRates}
              value={selectedSampleRate || ''}
              onChange={(value) => onSampleRateChange?.(String(value))}
            />
          </div>
          <div>
            <label className="block text-sm font-medium text-gray-300 mb-2">Buffer Size</label>
            <Dropdown
              options={bufferSizes}
              value={selectedBufferSize || ''}
              onChange={(value) => onBufferSizeChange?.(String(value))}
            />
          </div>
        </div>
      </div>

      {/* Display Settings */}
      <div className="rounded-lg bg-gray-800 border border-gray-700 p-4">
        <h3 className="text-lg font-bold text-white mb-4">Display Settings</h3>
        <div className="space-y-4">
          <div className="flex items-center justify-between">
            <label className="text-sm font-medium text-gray-300">Dark Theme</label>
            <ToggleSwitch
              checked={theme === 'dark'}
              onChange={(checked) => onThemeChange?.(checked ? 'dark' : 'light')}
            />
          </div>
          <div>
            <label className="block text-sm font-medium text-gray-300 mb-2">Scaling: {scaling}%</label>
            <input
              type="range"
              min="80"
              max="150"
              value={scaling}
              onChange={(e) => onScalingChange?.(parseInt(e.target.value))}
              className="w-full"
            />
          </div>
        </div>
      </div>

      {/* MIDI Settings */}
      {midiDevices.length > 0 && (
        <div className="rounded-lg bg-gray-800 border border-gray-700 p-4">
          <h3 className="text-lg font-bold text-white mb-4">MIDI Settings</h3>
          <div>
            <label className="block text-sm font-medium text-gray-300 mb-2">MIDI Device</label>
            <Dropdown
              options={midiDevices}
              value={selectedMidiDevice || ''}
              onChange={(value) => onMidiDeviceChange?.(String(value))}
              placeholder="Select MIDI device..."
            />
          </div>
        </div>
      )}
    </div>
  );
};
