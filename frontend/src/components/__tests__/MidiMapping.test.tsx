import { render, screen, waitFor } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { MidiMapping } from '../MidiMapping';
import type { EffectDescriptor, MidiState } from '../../services/api';

const { getMidi, setMidiDevice, clearMidiMapping, learnMidi } = vi.hoisted(() => ({
  getMidi: vi.fn(),
  setMidiDevice: vi.fn(),
  clearMidiMapping: vi.fn(),
  learnMidi: vi.fn(),
}));

vi.mock('../../services/api', async (importOriginal) => ({
  ...(await importOriginal<typeof import('../../services/api')>()),
  getMidi,
  setMidiDevice,
  clearMidiMapping,
  learnMidi,
}));

const effects: EffectDescriptor[] = [
  {
    id: 'overdrive',
    label: 'Overdrive',
    description: '',
    enabled: true,
    toggleable: true,
    parameters: [
      {
        id: 'drivePct',
        label: 'Drive',
        unit: '%',
        min: 0,
        max: 100,
        step: 0.5,
        default: 0,
        type: 'float',
        value: 25,
      },
    ],
  },
  {
    id: 'cabinet',
    label: 'Cabinet',
    description: '',
    enabled: true,
    toggleable: true,
    parameters: [
      {
        id: 'irFile',
        label: 'Impulse Response',
        unit: '',
        min: 0,
        max: 0,
        step: 1,
        default: 0,
        type: 'text',
        value: '',
        options: ['Marshall'],
      },
    ],
  },
  {
    id: 'master',
    label: 'Master',
    description: '',
    enabled: true,
    toggleable: false,
    parameters: [
      {
        id: 'volumeDb',
        label: 'Volume',
        unit: 'dB',
        min: -60,
        max: 12,
        step: 0.1,
        default: 0,
        type: 'float',
        value: 0,
      },
    ],
  },
];

function makeState(overrides: Partial<MidiState> = {}): MidiState {
  return {
    devices: ['FCB1010', 'nanoKONTROL2'],
    current: 'FCB1010',
    open: true,
    mappings: [],
    learning: null,
    lastCc: -1,
    lastValue: 0,
    ...overrides,
  };
}

describe('MidiMapping', () => {
  beforeEach(() => {
    getMidi.mockResolvedValue(makeState());
    setMidiDevice.mockResolvedValue(makeState());
    clearMidiMapping.mockResolvedValue(makeState());
    learnMidi.mockResolvedValue(makeState());
  });

  afterEach(() => {
    vi.clearAllMocks();
  });

  async function renderPanel(disabled = false) {
    const view = render(<MidiMapping effects={effects} disabled={disabled} />);
    await waitFor(() => expect(getMidi).toHaveBeenCalled());
    return view;
  }

  it('lists the MIDI inputs the engine reports', async () => {
    await renderPanel();

    const select = await screen.findByLabelText('Perangkat');
    expect(select).toHaveValue('FCB1010');
    expect(screen.getByRole('option', { name: 'nanoKONTROL2' })).toBeInTheDocument();
  });

  it('offers each effect switch as well as its parameters', async () => {
    // Turning a pedal on and off is the most useful thing to put under a
    // footswitch, and it is not a parameter, so it would otherwise be
    // unreachable from this panel.
    await renderPanel();

    expect(screen.getByRole('option', { name: 'Overdrive — On/Off' })).toBeInTheDocument();
    expect(screen.getByRole('option', { name: 'Overdrive — Drive' })).toBeInTheDocument();
  });

  it('leaves out a stage that is always in the path', async () => {
    await renderPanel();

    expect(screen.queryByRole('option', { name: 'Master — On/Off' })).not.toBeInTheDocument();
    expect(screen.getByRole('option', { name: 'Master — Volume' })).toBeInTheDocument();
  });

  it('leaves out parameters a controller sweep makes no sense for', async () => {
    // Nothing sensible maps CC 0..127 onto a list of filenames.
    await renderPanel();

    expect(
      screen.queryByRole('option', { name: 'Cabinet — Impulse Response' }),
    ).not.toBeInTheDocument();
  });

  it('will not arm learn until a target is chosen', async () => {
    await renderPanel();

    expect(screen.getByRole('button', { name: 'MIDI Learn' })).toBeDisabled();
  });

  it('arms learn against the chosen parameter', async () => {
    const user = userEvent.setup();
    await renderPanel();

    await user.selectOptions(screen.getByLabelText('Pasang kontrol ke'), 'param.overdrive.drivePct');
    await user.click(screen.getByRole('button', { name: 'MIDI Learn' }));

    expect(learnMidi).toHaveBeenCalledWith({
      kind: 'parameter',
      effect: 'overdrive',
      parameter: 'drivePct',
      mode: 'continuous',
    });
  });

  it('arms learn against a scene, for a footswitch under it', async () => {
    const user = userEvent.setup();
    await renderPanel();

    await user.selectOptions(screen.getByLabelText('Pasang kontrol ke'), 'scene.2');
    await user.click(screen.getByRole('button', { name: 'MIDI Learn' }));

    expect(learnMidi).toHaveBeenCalledWith({ kind: 'scene', index: 2, mode: 'toggle' });
  });

  it('arms an effect switch as a toggle, not a continuous sweep', async () => {
    // A footswitch sends 127 on press and 0 on release; a continuous mapping
    // would mean holding the switch down to keep the effect on.
    const user = userEvent.setup();
    await renderPanel();

    await user.selectOptions(screen.getByLabelText('Pasang kontrol ke'), 'param.overdrive.enabled');
    await user.click(screen.getByRole('button', { name: 'MIDI Learn' }));

    expect(learnMidi).toHaveBeenCalledWith({
      kind: 'parameter',
      effect: 'overdrive',
      parameter: 'enabled',
      mode: 'toggle',
    });
  });

  it('can cancel a learn that is waiting', async () => {
    getMidi.mockResolvedValue(
      makeState({
        learning: {
          cc: -1,
          kind: 'parameter',
          effect: 'overdrive',
          parameter: 'drivePct',
          index: -1,
          mode: 'continuous',
        },
      }),
    );

    const user = userEvent.setup();
    await renderPanel();

    expect(await screen.findByText(/Menunggu/)).toBeInTheDocument();

    await user.click(screen.getByRole('button', { name: 'Batal' }));

    expect(learnMidi).toHaveBeenCalledWith();
  });

  it('names what it is waiting to bind, not just its id', async () => {
    getMidi.mockResolvedValue(
      makeState({
        learning: {
          cc: -1,
          kind: 'parameter',
          effect: 'overdrive',
          parameter: 'drivePct',
          index: -1,
          mode: 'continuous',
        },
      }),
    );

    await renderPanel();

    // Scoped to the status line: the same label is also an option in the
    // assign dropdown, and matching that would prove nothing.
    const status = await screen.findByRole('status');
    expect(status).toHaveTextContent('Overdrive — Drive');
  });

  it('lists what is bound and can unbind it', async () => {
    getMidi.mockResolvedValue(
      makeState({
        mappings: [
          {
            cc: 7,
            kind: 'parameter',
            effect: 'overdrive',
            parameter: 'drivePct',
            index: -1,
            mode: 'continuous',
          },
        ],
      }),
    );

    const user = userEvent.setup();
    const { container } = await renderPanel();

    const row = await waitFor(() => {
      const found = container.querySelector('.midi__item');
      expect(found).not.toBeNull();
      return found as HTMLElement;
    });

    expect(row).toHaveTextContent('CC 7');
    expect(row).toHaveTextContent('Overdrive — Drive');
    expect(row).toHaveTextContent('Kontinu');

    await user.click(screen.getByRole('button', { name: 'Hapus CC 7' }));

    expect(clearMidiMapping).toHaveBeenCalledWith(7);
  });

  it('still names a binding whose effect is no longer in the chain', async () => {
    // Dropping the row would look like the mapping was gone when it is still
    // stored and still firing.
    getMidi.mockResolvedValue(
      makeState({
        mappings: [
          { cc: 3, kind: 'parameter', effect: 'ghost', parameter: 'mystery', index: -1, mode: 'toggle' },
        ],
      }),
    );

    await renderPanel();

    expect(await screen.findByText('ghost — mystery')).toBeInTheDocument();
  });

  it('keeps an unplugged controller in the list rather than losing its name', async () => {
    getMidi.mockResolvedValue(makeState({ devices: [], current: 'FCB1010', open: false }));

    await renderPanel();

    expect(await screen.findByRole('option', { name: /FCB1010 \(tidak terhubung\)/ })).toBeInTheDocument();
    expect(screen.getByText(/Tidak ada perangkat MIDI terdeteksi/)).toBeInTheDocument();
  });

  it('shows the last controller seen, so a silent rig can be told from an unmapped one', async () => {
    getMidi.mockResolvedValue(makeState({ lastCc: 11, lastValue: 64 }));

    await renderPanel();

    expect(await screen.findByText('Terakhir: CC 11 = 64')).toBeInTheDocument();
  });

  it('changes the input device', async () => {
    const user = userEvent.setup();
    await renderPanel();

    await user.selectOptions(await screen.findByLabelText('Perangkat'), 'nanoKONTROL2');

    expect(setMidiDevice).toHaveBeenCalledWith('nanoKONTROL2');
  });

  it('surfaces what the engine refused rather than failing silently', async () => {
    setMidiDevice.mockRejectedValue(new Error('Tidak bisa membuka perangkat MIDI: FCB1010'));

    const user = userEvent.setup();
    await renderPanel();

    await user.selectOptions(await screen.findByLabelText('Perangkat'), 'nanoKONTROL2');

    expect(
      await screen.findByText('Tidak bisa membuka perangkat MIDI: FCB1010'),
    ).toBeInTheDocument();
  });
});
