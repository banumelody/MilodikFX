import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { TunerDisplay } from '../TunerDisplay';
import type { TunerReading } from '../../services/api';

// subscribeTuner is mocked rather than the fetch under it: the real one polls on
// a timer, and a test that waits for a 60 ms tick is a test that will one day
// fail on a busy machine for no reason. `next` is whatever the engine is
// pretending to report when the panel subscribes.
const { setTunerEnabled, subscribeTuner, state } = vi.hoisted(() => {
  const shared: { next: unknown } = { next: null };

  return {
    state: shared,
    setTunerEnabled: vi.fn(),
    subscribeTuner: vi.fn((onReading: (reading: unknown) => void) => {
      if (shared.next != null) onReading(shared.next);
      return () => {};
    }),
  };
});

vi.mock('../../services/api', async (importOriginal) => ({
  ...(await importOriginal<typeof import('../../services/api')>()),
  setTunerEnabled,
  subscribeTuner,
}));

function reading(overrides: Partial<TunerReading> = {}): TunerReading {
  return {
    enabled: true,
    note: 'A2',
    midiNote: 45,
    frequency: 110,
    cents: 0,
    confidence: 0.8,
    detected: true,
    ...overrides,
  };
}

async function start(next: TunerReading | null = reading()) {
  state.next = next;

  const user = userEvent.setup();
  const view = render(<TunerDisplay />);
  await user.click(screen.getByRole('button', { name: 'Mulai' }));

  return { user, ...view };
}

describe('TunerDisplay', () => {
  beforeEach(() => {
    state.next = null;
    setTunerEnabled.mockResolvedValue(reading({ enabled: false, detected: false }));
  });

  afterEach(() => {
    vi.clearAllMocks();
  });

  it('leaves the analyser off until asked', () => {
    render(<TunerDisplay />);

    expect(setTunerEnabled).not.toHaveBeenCalled();
    expect(subscribeTuner).not.toHaveBeenCalled();
    expect(screen.getByRole('button', { name: 'Mulai' })).toBeInTheDocument();
  });

  it('switches the engine analyser on when opened', async () => {
    await start();

    expect(setTunerEnabled).toHaveBeenCalledWith(true);
  });

  it('switches it off again when closed, rather than leaving it burning CPU', async () => {
    const { user } = await start();

    await user.click(screen.getByRole('button', { name: 'Berhenti' }));

    expect(setTunerEnabled).toHaveBeenLastCalledWith(false);
  });

  it('shows the note and its deviation once the engine reports one', async () => {
    await start(reading({ cents: 12.4 }));

    expect(screen.getByText('A2')).toBeInTheDocument();
    expect(screen.getByText('110.00 Hz')).toBeInTheDocument();
    expect(screen.getByText('+12.4 cent')).toBeInTheDocument();
  });

  it('calls a note within five cents in tune', async () => {
    const { container } = await start(reading({ cents: 3 }));

    expect(container.querySelector('.tuner--in-tune')).toBeInTheDocument();
    expect(screen.getByText('Pas.')).toBeInTheDocument();
  });

  it('says which way to turn the peg when the note is out', async () => {
    await start(reading({ cents: -22 }));

    expect(screen.getByText('Terlalu rendah — kencangkan.')).toBeInTheDocument();
  });

  it('says the other way when the note is sharp', async () => {
    await start(reading({ cents: 22 }));

    expect(screen.getByText('Terlalu tinggi — kendurkan.')).toBeInTheDocument();
  });

  it('moves the needle in proportion to the deviation', async () => {
    const { container } = await start(reading({ cents: 25 }));

    // 25 cents sharp is three quarters of the way across a -50..+50 scale.
    expect(container.querySelector<HTMLElement>('.tuner__needle')?.style.left).toBe('75%');
  });

  it('pins the needle at the end of the scale rather than off it', async () => {
    const { container } = await start(reading({ cents: -400 }));

    expect(container.querySelector<HTMLElement>('.tuner__needle')?.style.left).toBe('0%');
  });

  it('shows no note at all when nothing is being played', async () => {
    const { container } = await start(reading({ detected: false, note: '', midiNote: -1 }));

    expect(screen.getByText('Petik satu senar saja, biarkan berdering.')).toBeInTheDocument();

    // No needle either: one parked at dead centre would read as "in tune".
    expect(container.querySelector('.tuner__needle')).toBeNull();
    expect(container.querySelector('.tuner__note')).toHaveTextContent('--');
  });

  it('does not claim a stale note is in tune after being closed', async () => {
    const { user, container } = await start(reading({ cents: 0 }));

    expect(container.querySelector('.tuner--in-tune')).toBeInTheDocument();

    await user.click(screen.getByRole('button', { name: 'Berhenti' }));

    expect(container.querySelector('.tuner--in-tune')).toBeNull();
    expect(screen.queryByText('A2')).not.toBeInTheDocument();
  });

  it('cannot be started while the engine is unreachable', () => {
    render(<TunerDisplay disabled />);

    expect(screen.getByRole('button', { name: 'Mulai' })).toBeDisabled();
  });
});
