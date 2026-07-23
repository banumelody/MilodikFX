import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import { beforeEach, describe, expect, it, vi } from 'vitest';

import { ModulationPanel } from '../ModulationPanel';
import type { EffectDescriptor, ModifiersState } from '../../services/api';

vi.mock('../../services/api', () => ({
  getModifiers: vi.fn(),
  setModifier: vi.fn(),
  clearModifier: vi.fn(),
}));

import { clearModifier, getModifiers, setModifier } from '../../services/api';

const emptySlots: ModifiersState = {
  modifiers: [0, 1, 2, 3].map((slot) => ({
    slot,
    active: false,
    effect: '',
    parameter: '',
    source: 'lfoSine' as const,
    low: 0,
    high: 0,
    rateHz: 1,
    expressionCc: -1,
    syncDivision: 0,
    baseOffset: 0,
    base: 0,
  })),
};

const effects: EffectDescriptor[] = [
  {
    id: 'overdrive',
    label: 'Overdrive',
    description: '',
    enabled: true,
    toggleable: true,
    parameters: [
      { id: 'drivePct', label: 'Drive', unit: '%', min: 0, max: 100, step: 1, default: 0, type: 'float', value: 25 },
      { id: 'type', label: 'Tipe', unit: '', min: 0, max: 11, step: 1, default: 0, type: 'float', value: 0 },
    ],
  },
];

function renderPanel() {
  const onModifiersChanged = vi.fn();
  render(<ModulationPanel effects={effects} onModifiersChanged={onModifiersChanged} />);
  return { onModifiersChanged };
}

beforeEach(() => {
  vi.clearAllMocks();
  vi.mocked(getModifiers).mockResolvedValue(emptySlots);
  vi.mocked(setModifier).mockResolvedValue(emptySlots);
  vi.mocked(clearModifier).mockResolvedValue(emptySlots);
});

describe('ModulationPanel', () => {
  it('offers the numeric parameters as sweep targets', async () => {
    renderPanel();
    expect(await screen.findByRole('option', { name: 'Overdrive — Drive' })).toBeInTheDocument();
  });

  it('adds a modifier into the first free slot for the chosen target and source', async () => {
    const { onModifiersChanged } = renderPanel();
    await screen.findByRole('option', { name: 'Overdrive — Drive' });

    fireEvent.change(screen.getByLabelText('Parameter'), { target: { value: 'overdrive.drivePct' } });
    fireEvent.change(screen.getByLabelText('Sumber'), { target: { value: 'envelope' } });
    fireEvent.click(screen.getByRole('button', { name: 'Tambah modifier' }));

    await waitFor(() => expect(setModifier).toHaveBeenCalled());

    const [slot, body] = vi.mocked(setModifier).mock.calls[0];
    expect(slot).toBe(0);
    expect(body.effect).toBe('overdrive');
    expect(body.parameter).toBe('drivePct');
    expect(body.source).toBe('envelope');

    await waitFor(() => expect(onModifiersChanged).toHaveBeenCalled());
  });

  it('offers an expression-pedal source with a CC field', async () => {
    renderPanel();
    await screen.findByRole('option', { name: 'Overdrive — Drive' });

    fireEvent.change(screen.getByLabelText('Parameter'), { target: { value: 'overdrive.drivePct' } });
    fireEvent.change(screen.getByLabelText('Sumber'), { target: { value: 'expression' } });

    // The CC field appears for an expression source; the Sync field does not.
    fireEvent.change(screen.getByLabelText('CC pedal'), { target: { value: '7' } });
    expect(screen.queryByLabelText('Sync')).not.toBeInTheDocument();

    fireEvent.click(screen.getByRole('button', { name: 'Tambah modifier' }));
    await waitFor(() => expect(setModifier).toHaveBeenCalled());

    const [, body] = vi.mocked(setModifier).mock.calls[0];
    expect(body.source).toBe('expression');
    expect(body.expressionCc).toBe(7);
  });

  it('locks an LFO to the tempo through the sync division', async () => {
    renderPanel();
    await screen.findByRole('option', { name: 'Overdrive — Drive' });

    fireEvent.change(screen.getByLabelText('Parameter'), { target: { value: 'overdrive.drivePct' } });
    // lfoSine is the default source, so the Sync field is present.
    fireEvent.change(screen.getByLabelText('Sync'), { target: { value: '3' } }); // 1/4

    fireEvent.click(screen.getByRole('button', { name: 'Tambah modifier' }));
    await waitFor(() => expect(setModifier).toHaveBeenCalled());

    const [, body] = vi.mocked(setModifier).mock.calls[0];
    expect(body.syncDivision).toBe(3);
  });

  it('lists an active modifier and can clear it', async () => {
    vi.mocked(getModifiers).mockResolvedValue({
      modifiers: [
        { slot: 0, active: true, effect: 'overdrive', parameter: 'drivePct', source: 'lfoSine', low: 0, high: 100, rateHz: 2, expressionCc: -1, syncDivision: 0, baseOffset: 0, base: 50 },
        ...emptySlots.modifiers.slice(1),
      ],
    });

    renderPanel();

    const clear = await screen.findByRole('button', { name: 'Hapus modifier 0' });
    fireEvent.click(clear);

    await waitFor(() => expect(clearModifier).toHaveBeenCalledWith(0));
  });
});
