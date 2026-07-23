import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import { beforeEach, describe, expect, it, vi } from 'vitest';

import { PerformView } from '../PerformView';
import type { Levels, ParameterDescriptor, ScenesState } from '../../services/api';

vi.mock('../../services/api', () => ({
  getScenes: vi.fn(),
  recallScene: vi.fn(),
  setTunerEnabled: vi.fn(() => Promise.resolve({})),
  subscribeTuner: vi.fn(() => () => {}),
  getLooper: vi.fn(() =>
    Promise.resolve({
      state: 'empty',
      hasLoop: false,
      loopSeconds: 0,
      position: 0,
      level: 100,
      maxSeconds: 60,
    }),
  ),
  looperAction: vi.fn(() =>
    Promise.resolve({
      state: 'recording',
      hasLoop: false,
      loopSeconds: 0,
      position: 0,
      level: 100,
      maxSeconds: 60,
    }),
  ),
  setLooperLevel: vi.fn(),
}));

import { getScenes, recallScene, setTunerEnabled } from '../../services/api';

const scenesState: ScenesState = {
  active: 0,
  scenes: [
    { index: 0, name: 'Clean', enabled: {}, populated: true },
    { index: 1, name: 'Crunch', enabled: {}, populated: true },
    { index: 2, name: 'Lead', enabled: {}, populated: true },
    { index: 3, name: 'Solo', enabled: {}, populated: true },
  ],
};

const levels: Levels = {
  inputLevel: -20,
  chainInputLevel: -18,
  outputLevel: -12,
  gateGain: 1,
  compressorReductionDb: 0,
  limiterReductionDb: 0,
  cpuPercent: 5,
  sampleRate: 96000,
  bufferSize: 32,
  audioRunning: true,
  floorDb: -100,
};

const bpm: ParameterDescriptor = {
  id: 'bpm',
  label: 'BPM',
  unit: '',
  min: 40,
  max: 240,
  step: 1,
  default: 120,
  type: 'float',
  value: 120,
};

function renderPerform(overrides: Partial<Parameters<typeof PerformView>[0]> = {}) {
  const props = {
    levels,
    effects: [],
    presets: ['Rock', 'Jazz', 'Metal'],
    selectedPreset: 'Jazz',
    onLoadPreset: vi.fn(),
    bpm,
    onParameterChange: vi.fn(),
    isBypassed: false,
    isMuted: false,
    onToggleBypass: vi.fn(),
    onToggleMute: vi.fn(),
    offline: false,
    onScenesRecalled: vi.fn(),
    ...overrides,
  };
  render(<PerformView {...props} />);
  return props;
}

beforeEach(() => {
  vi.clearAllMocks();
  vi.mocked(getScenes).mockResolvedValue(scenesState);
  vi.mocked(recallScene).mockResolvedValue(scenesState);
});

describe('PerformView', () => {
  it('shows four giant scene buttons and the current preset', async () => {
    renderPerform();

    expect(await screen.findByText('Clean')).toBeInTheDocument();
    expect(screen.getByText('Solo')).toBeInTheDocument();
    expect(screen.getByText('Jazz')).toBeInTheDocument();
  });

  it('recalls a scene when its button is clicked', async () => {
    const props = renderPerform();

    fireEvent.click(await screen.findByText('Crunch'));

    await waitFor(() => expect(recallScene).toHaveBeenCalledWith(1));
    await waitFor(() => expect(props.onScenesRecalled).toHaveBeenCalled());
  });

  it('recalls a scene from a number key', async () => {
    renderPerform();
    await screen.findByText('Clean');

    fireEvent.keyDown(window, { key: '3' });

    await waitFor(() => expect(recallScene).toHaveBeenCalledWith(2));
  });

  it('steps presets forward and wraps around', () => {
    const props = renderPerform();

    fireEvent.click(screen.getByLabelText('Preset berikutnya'));
    expect(props.onLoadPreset).toHaveBeenCalledWith('Metal');
  });

  it('wraps from the last preset back to the first', () => {
    const props = renderPerform({ selectedPreset: 'Metal' });

    fireEvent.click(screen.getByLabelText('Preset berikutnya'));
    expect(props.onLoadPreset).toHaveBeenCalledWith('Rock');
  });

  it('steps presets with the arrow keys', () => {
    const props = renderPerform();

    fireEvent.keyDown(window, { key: 'ArrowLeft' });
    expect(props.onLoadPreset).toHaveBeenCalledWith('Rock');
  });

  it('toggles bypass and mute', () => {
    const props = renderPerform();

    fireEvent.click(screen.getByRole('button', { name: 'Bypass' }));
    expect(props.onToggleBypass).toHaveBeenCalled();

    fireEvent.click(screen.getByRole('button', { name: 'Mute' }));
    expect(props.onToggleMute).toHaveBeenCalled();
  });

  it('badges a scene button with the channel letter of a multi-channel effect', async () => {
    vi.mocked(getScenes).mockResolvedValue({
      active: 0,
      scenes: [
        { index: 0, name: 'Clean', enabled: { overdrive: true }, channels: { overdrive: 1 }, populated: true },
        { index: 1, name: 'Crunch', enabled: {}, populated: true },
        { index: 2, name: 'Lead', enabled: {}, populated: true },
        { index: 3, name: 'Solo', enabled: {}, populated: true },
      ],
    });

    renderPerform({
      effects: [
        {
          id: 'overdrive',
          label: 'Overdrive',
          description: '',
          enabled: true,
          toggleable: true,
          parameters: [],
          channels: ['A', 'B', 'C', 'D'],
        },
      ],
    });

    await screen.findByText('Clean');

    // Effect abbreviation "Ov" plus the channel letter "B" (index 1).
    await waitFor(() => {
      const badge = document.querySelector('.perform__chan-badge');
      expect(badge?.textContent).toBe('OvB');
    });
  });

  it('switches the tuner on, replacing the scene grid', async () => {
    renderPerform();
    await screen.findByText('Clean');

    fireEvent.click(screen.getByRole('button', { name: 'Tuner' }));

    await waitFor(() => expect(setTunerEnabled).toHaveBeenCalledWith(true));
    // The scene grid is gone while tuning.
    expect(screen.queryByText('Clean')).not.toBeInTheDocument();
  });
});
