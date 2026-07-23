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

    fireEvent.click(rec);

    await waitFor(() => expect(looperAction).toHaveBeenCalledWith('record'));
  });

  it('labels the button by state — Overdub while playing, and can clear', async () => {
    vi.mocked(getLooper).mockResolvedValue(playing);
    render(<LooperPanel />);

    expect(await screen.findByRole('button', { name: 'Overdub' })).toBeInTheDocument();
    // A loop exists, so Hapus is enabled.
    fireEvent.click(screen.getByRole('button', { name: 'Hapus' }));

    await waitFor(() => expect(looperAction).toHaveBeenCalledWith('clear'));
  });
});
