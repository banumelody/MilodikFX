import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import { beforeEach, describe, expect, it, vi } from 'vitest';

import { LooperPanel } from '../LooperPanel';
import type { LooperInfo } from '../../services/api';

vi.mock('../../services/api', () => ({
  getLooper: vi.fn(),
  looperAction: vi.fn(),
  setLooperLevel: vi.fn(),
}));

import { getLooper, looperAction, setLooperLevel } from '../../services/api';

const empty: LooperInfo = {
  state: 'empty',
  hasLoop: false,
  loopSeconds: 0,
  position: 0,
  level: 100,
  maxSeconds: 60,
};

const playing: LooperInfo = {
  state: 'playing',
  hasLoop: true,
  loopSeconds: 4.2,
  position: 0.5,
  level: 80,
  maxSeconds: 60,
};

beforeEach(() => {
  vi.clearAllMocks();
  vi.mocked(getLooper).mockResolvedValue(empty);
  vi.mocked(looperAction).mockResolvedValue({ ...empty, state: 'recording' });
  vi.mocked(setLooperLevel).mockResolvedValue(empty);
});

describe('LooperPanel', () => {
  it('shows the record button and empty state', async () => {
    render(<LooperPanel />);

    expect(await screen.findByRole('button', { name: 'Rekam' })).toBeInTheDocument();
    expect(screen.getByText('Kosong')).toBeInTheDocument();
  });

  it('requests the record action on click', async () => {
    render(<LooperPanel />);
    const rec = await screen.findByRole('button', { name: 'Rekam' });

    // Enabled, not merely present. The panel renders the button straight away
    // but holds every control disabled until the first /api/looper lands, so a
    // click on a found-but-disabled button silently does nothing -- which is
    // how this went red on CI with "Number of calls: 0" while passing here.
    await waitFor(() => expect(rec).toBeEnabled());

    fireEvent.click(rec);

    await waitFor(() => expect(looperAction).toHaveBeenCalledWith('record'));
  });

  it('labels the button by state — Overdub while playing, and can clear', async () => {
    vi.mocked(getLooper).mockResolvedValue(playing);
    render(<LooperPanel />);

    expect(await screen.findByRole('button', { name: 'Overdub' })).toBeInTheDocument();
    // A loop exists, so Hapus is enabled -- once the state has arrived.
    const clear = screen.getByRole('button', { name: 'Hapus' });
    await waitFor(() => expect(clear).toBeEnabled());
    fireEvent.click(clear);

    await waitFor(() => expect(looperAction).toHaveBeenCalledWith('clear'));
  });

  it('takes its state from the meter stream and stops polling entirely', async () => {
    // The whole point of moving it into the level payload: the panel used to
    // open four sockets a second for a payload that fits in a stream already
    // running at ~22 Hz.
    render(<LooperPanel streamed={playing} />);

    expect(await screen.findByRole('button', { name: 'Overdub' })).toBeInTheDocument();
    expect(screen.getByText('Main')).toBeInTheDocument();
    expect(getLooper).not.toHaveBeenCalled();
  });

  it('still polls when the engine does not report a looper in the stream', async () => {
    // An older engine, or one that simply has no looper. Same graceful fallback
    // the level stream itself has.
    render(<LooperPanel />);

    expect(await screen.findByRole('button', { name: 'Rekam' })).toBeInTheDocument();
    await waitFor(() => expect(getLooper).toHaveBeenCalled());
  });
});
