import { fireEvent, render, screen } from '@testing-library/react';
import { afterEach, describe, expect, it, vi } from 'vitest';

import { LanguageProvider } from '../../i18n';
import { EffectRack } from '../EffectRack';
import type { EffectDescriptor } from '../../services/api';

const overdrive: EffectDescriptor = {
  id: 'overdrive',
  label: 'Overdrive',
  description: 'Cubic soft clipper',
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
    {
      id: 'asymmetry',
      label: 'Asymmetry',
      unit: '',
      min: 0,
      max: 1,
      step: 0.01,
      default: 0,
      type: 'float',
      value: 0,
    },
    {
      // Voicing selector. Which controls appear below it depends on this.
      id: 'type',
      label: 'Tipe',
      unit: '',
      min: 0,
      max: 11,
      step: 1,
      default: 0,
      type: 'float',
      value: 0,
    },
    {
      id: 'tonePct',
      label: 'Tone',
      unit: '%',
      min: 0,
      max: 100,
      step: 1,
      default: 50,
      type: 'float',
      value: 50,
    },
    {
      id: 'bassDb',
      label: 'Bass',
      unit: 'dB',
      min: -12,
      max: 12,
      step: 0.1,
      default: 0,
      type: 'float',
      value: 0,
    },
    {
      id: 'levelPct',
      label: 'Level',
      unit: '%',
      min: 0,
      max: 100,
      step: 0.5,
      default: 100,
      type: 'float',
      value: 100,
    },
    {
      // Enum: rendered as a labelled choice rather than a knob.
      id: 'oversampling',
      label: 'Oversampling',
      unit: 'x',
      min: 0,
      max: 3,
      step: 1,
      default: 1,
      type: 'float',
      value: 1,
    },
  ],
};

/** The same overdrive with a different voicing selected. */
function overdriveAs(type: number): EffectDescriptor {
  return {
    ...overdrive,
    parameters: overdrive.parameters.map((p) =>
      p.id === 'type' ? { ...p, value: type } : p,
    ),
  };
}

const cabinet: EffectDescriptor = {
  id: 'cabinet',
  label: 'Cabinet',
  description: 'Speaker emulation',
  enabled: true,
  toggleable: true,
  parameters: [
    {
      id: 'irEnabled',
      label: 'Pakai IR',
      unit: '',
      min: 0,
      max: 1,
      step: 1,
      default: 0,
      type: 'bool',
      value: 0,
    },
    {
      id: 'irFile',
      label: 'Impulse Response',
      unit: '',
      min: 0,
      max: 1,
      step: 1,
      default: 0,
      type: 'text',
      value: '',
      options: ['Marshall 4x12', 'Vox AC30'],
    },
  ],
};

const input: EffectDescriptor = {
  id: 'input',
  label: 'Input',
  description: 'Channel mapping',
  enabled: true,
  toggleable: false,
  parameters: [
    {
      id: 'mode',
      label: 'Mode',
      unit: '',
      min: 0,
      max: 3,
      step: 1,
      default: 0,
      type: 'float',
      value: 0,
    },
  ],
};

function renderRack(effect: EffectDescriptor) {
  const onParameterChange = vi.fn();
  const onEnabledChange = vi.fn();
  const onChannelSelect = vi.fn();

  render(
    <EffectRack
      effect={effect}
      onParameterChange={onParameterChange}
      onEnabledChange={onEnabledChange}
      onChannelSelect={onChannelSelect}
    />,
  );

  return { onParameterChange, onEnabledChange, onChannelSelect };
}

describe('EffectRack drive voicings', () => {
  it('offers the voicings by name rather than by number', () => {
    renderRack(overdrive);

    const select = screen.getByRole('combobox', { name: 'Tipe' });
    expect(select).toHaveValue('0');
    expect(screen.getByRole('option', { name: 'Tube Screamer' })).toBeInTheDocument();
    expect(screen.getByRole('option', { name: 'Marshall-in-a-Box' })).toBeInTheDocument();
  });

  it('shows the Custom voicing its own controls', () => {
    // Custom is the pre-voicing behaviour, so it keeps Asymmetry and has no
    // Tone knob at all.
    renderRack(overdrive);

    expect(screen.getByRole('slider', { name: 'Asymmetry' })).toBeInTheDocument();
    expect(screen.queryByRole('slider', { name: 'Tone' })).not.toBeInTheDocument();
    expect(screen.queryByRole('slider', { name: 'Bass' })).not.toBeInTheDocument();
  });

  it('gives a Tube Screamer a Tone knob and takes away Asymmetry', () => {
    renderRack(overdriveAs(1));

    expect(screen.getByRole('slider', { name: 'Tone' })).toBeInTheDocument();
    expect(screen.queryByRole('slider', { name: 'Asymmetry' })).not.toBeInTheDocument();
    expect(screen.queryByRole('slider', { name: 'Bass' })).not.toBeInTheDocument();
  });

  it('calls the gain control what the original pedal called it', () => {
    // A Bluesbreaker has a Gain knob and a Volume knob, not Drive and Level.
    renderRack(overdriveAs(2));

    expect(screen.getByRole('slider', { name: 'Gain' })).toBeInTheDocument();
    expect(screen.getByRole('slider', { name: 'Volume' })).toBeInTheDocument();
    expect(screen.queryByRole('slider', { name: 'Drive' })).not.toBeInTheDocument();
  });

  it('gives a transparent drive Bass and Treble instead of Tone', () => {
    renderRack(overdriveAs(4));

    expect(screen.getByRole('slider', { name: 'Bass' })).toBeInTheDocument();
    expect(screen.queryByRole('slider', { name: 'Tone' })).not.toBeInTheDocument();
  });

  it('shows a clean boost almost nothing', () => {
    renderRack(overdriveAs(8));

    expect(screen.getByRole('slider', { name: 'Boost' })).toBeInTheDocument();
    expect(screen.queryByRole('slider', { name: 'Tone' })).not.toBeInTheDocument();
    expect(screen.queryByRole('slider', { name: 'Asymmetry' })).not.toBeInTheDocument();
  });

  it('names a Centaur Gain, Treble and Output', () => {
    renderRack(overdriveAs(9));

    expect(screen.getByRole('slider', { name: 'Gain' })).toBeInTheDocument();
    expect(screen.getByRole('slider', { name: 'Treble' })).toBeInTheDocument();
    expect(screen.getByRole('slider', { name: 'Output' })).toBeInTheDocument();
    expect(screen.queryByRole('slider', { name: 'Asymmetry' })).not.toBeInTheDocument();
  });

  it('names a RAT Distortion, Filter and Volume', () => {
    renderRack(overdriveAs(10));

    expect(screen.getByRole('slider', { name: 'Distortion' })).toBeInTheDocument();
    expect(screen.getByRole('slider', { name: 'Filter' })).toBeInTheDocument();
    expect(screen.getByRole('slider', { name: 'Volume' })).toBeInTheDocument();
  });

  it('names a Big Muff Sustain, Tone and Volume', () => {
    renderRack(overdriveAs(11));

    expect(screen.getByRole('slider', { name: 'Sustain' })).toBeInTheDocument();
    expect(screen.getByRole('slider', { name: 'Tone' })).toBeInTheDocument();
    expect(screen.getByRole('slider', { name: 'Volume' })).toBeInTheDocument();
  });

  it('writes the voicing choice as its index', () => {
    const { onParameterChange } = renderRack(overdrive);

    fireEvent.change(screen.getByRole('combobox', { name: 'Tipe' }), {
      target: { value: '5' },
    });

    expect(onParameterChange).toHaveBeenCalledWith('overdrive', 'type', 5);
  });

  it('nudges oversampling to a value that suits the chosen voicing', () => {
    const { onParameterChange } = renderRack(overdrive);

    // Big Muff (11) is a fuzz -- the most high harmonics, so it gets 4x (index 2)
    // rather than the 2x default a clean voicing would keep.
    fireEvent.change(screen.getByRole('combobox', { name: 'Tipe' }), {
      target: { value: '11' },
    });

    expect(onParameterChange).toHaveBeenCalledWith('overdrive', 'type', 11);
    expect(onParameterChange).toHaveBeenCalledWith('overdrive', 'oversampling', 2);
  });

  it('leaves oversampling alone for non-drive enum changes', () => {
    const { onParameterChange } = renderRack(overdrive);

    // The oversampling nudge is specific to the voicing selector; changing the
    // oversampling dropdown itself must not fire a second write.
    fireEvent.change(screen.getByRole('combobox', { name: 'Oversampling' }), {
      target: { value: '3' },
    });

    expect(onParameterChange).toHaveBeenCalledTimes(1);
    expect(onParameterChange).toHaveBeenCalledWith('overdrive', 'oversampling', 3);
  });

  it('falls back to the Custom layout for a voicing it does not know', () => {
    // A preset saved by a newer build must still render something usable
    // rather than an empty card.
    renderRack(overdriveAs(99));

    expect(screen.getByRole('slider', { name: 'Drive' })).toBeInTheDocument();
    expect(screen.getByRole('combobox', { name: 'Tipe' })).toBeInTheDocument();
  });

  it('keeps a modulated knob live with a MOD tag (base + offset)', () => {
    // A modifier owns this parameter, but under base + offset the knob still sets
    // the centre the sweep rides on -- so it stays interactive, tagged MOD.
    render(
      <EffectRack
        effect={overdrive}
        onParameterChange={vi.fn()}
        onEnabledChange={vi.fn()}
        modulatedParams={new Set(['overdrive.drivePct'])}
      />,
    );

    expect(screen.getByRole('slider', { name: 'Drive' })).not.toHaveAttribute('aria-disabled', 'true');
    expect(screen.getByText('MOD')).toBeInTheDocument();
  });
});

describe('EffectRack channels', () => {
  const withChannels = (active: number): EffectDescriptor => ({
    ...overdrive,
    channel: active,
    channels: ['A', 'B', 'C', 'D'],
  });

  it('shows four channel tabs with the active one selected', () => {
    renderRack(withChannels(1));

    const tabs = screen.getAllByRole('tab');
    expect(tabs).toHaveLength(4);
    expect(tabs[1]).toHaveAttribute('aria-selected', 'true');
    expect(tabs[0]).toHaveAttribute('aria-selected', 'false');
  });

  it('selects a channel by index when a tab is clicked', () => {
    const { onChannelSelect } = renderRack(withChannels(0));

    fireEvent.click(screen.getByRole('tab', { name: 'C' }));
    expect(onChannelSelect).toHaveBeenCalledWith('overdrive', 2);
  });

  it('shows no tabs for a stage that is always in the path', () => {
    // Input routing and master out have no bypass and no channels.
    renderRack({ ...withChannels(0), toggleable: false });

    expect(screen.queryByRole('tab')).not.toBeInTheDocument();
  });

  it('shows no tabs when the engine reports no channels', () => {
    renderRack(overdrive);

    expect(screen.queryByRole('tab')).not.toBeInTheDocument();
  });
});

describe('EffectRack', () => {
  it('renders every parameter the engine advertises', () => {
    renderRack(overdrive);

    expect(screen.getByRole('slider', { name: 'Drive' })).toBeInTheDocument();
    expect(screen.getByRole('slider', { name: 'Asymmetry' })).toBeInTheDocument();
    expect(screen.getByRole('combobox', { name: 'Oversampling' })).toBeInTheDocument();
    expect(screen.getByRole('switch', { name: 'Overdrive on/off' })).toBeInTheDocument();
  });

  it('reports knob movement with the effect and parameter id', () => {
    const { onParameterChange } = renderRack(overdrive);

    fireEvent.keyDown(screen.getByRole('slider', { name: 'Drive' }), { key: 'ArrowUp' });

    expect(onParameterChange).toHaveBeenLastCalledWith('overdrive', 'drivePct', 25.5);
  });

  it('renders an enum parameter as named choices, not a raw number', () => {
    const { onParameterChange } = renderRack(overdrive);

    const select = screen.getByRole('combobox', { name: 'Oversampling' });
    expect(screen.getByRole('option', { name: 'Mati' })).toBeInTheDocument();
    expect(screen.getByRole('option', { name: '8x' })).toBeInTheDocument();

    fireEvent.change(select, { target: { value: '3' } });
    expect(onParameterChange).toHaveBeenLastCalledWith('overdrive', 'oversampling', 3);
  });

  it('sends booleans as 0 or 1', () => {
    const { onParameterChange } = renderRack(cabinet);

    fireEvent.click(screen.getByRole('switch', { name: 'Pakai IR' }));

    expect(onParameterChange).toHaveBeenLastCalledWith('cabinet', 'irEnabled', 1);
  });

  it('offers the engine-reported files for a text parameter and sends the name', () => {
    // The choices come from the engine, never from a hardcoded list here, so a
    // file dropped into the folder shows up without a frontend change.
    const { onParameterChange } = renderRack(cabinet);

    expect(screen.getByRole('option', { name: 'Marshall 4x12' })).toBeInTheDocument();
    expect(screen.getByRole('option', { name: 'Vox AC30' })).toBeInTheDocument();

    fireEvent.change(screen.getByRole('combobox'), { target: { value: 'Vox AC30' } });

    expect(onParameterChange).toHaveBeenLastCalledWith('cabinet', 'irFile', 'Vox AC30');
  });

  it('lets a text parameter be cleared back to nothing', () => {
    const { onParameterChange } = renderRack(cabinet);

    fireEvent.change(screen.getByRole('combobox'), { target: { value: '' } });

    expect(onParameterChange).toHaveBeenLastCalledWith('cabinet', 'irFile', '');
  });

  it('toggles the whole effect', () => {
    const { onEnabledChange } = renderRack(overdrive);

    fireEvent.click(screen.getByRole('switch', { name: 'Overdrive on/off' }));

    expect(onEnabledChange).toHaveBeenLastCalledWith('overdrive', false);
  });

  it('renders the input routing as a labelled choice, not a knob', () => {
    const { onParameterChange } = renderRack(input);

    const select = screen.getByRole('combobox');
    expect(select).toBeInTheDocument();
    expect(screen.getByRole('option', { name: 'Mono - Input 1' })).toBeInTheDocument();
    expect(screen.getByRole('option', { name: 'Stereo' })).toBeInTheDocument();

    fireEvent.change(select, { target: { value: '3' } });
    expect(onParameterChange).toHaveBeenLastCalledWith('input', 'mode', 3);
  });

  it('draws no on/off switch for a stage the engine marks as not toggleable', () => {
    // The master output is the important case: a header switch there looks like
    // a bypass but mutes the whole app, which is how the output once went dead
    // with nothing on screen explaining why.
    renderRack(input);
    expect(screen.queryByRole('switch')).not.toBeInTheDocument();
  });

  it('renders a non-toggleable effect with its parameters still reachable', () => {
    const master: EffectDescriptor = {
      id: 'master',
      label: 'Master',
      description: 'Output level and safety limiter',
      enabled: true,
      toggleable: false,
      parameters: [
        {
          id: 'muted',
          label: 'Mute',
          unit: '',
          min: 0,
          max: 1,
          step: 1,
          default: 0,
          type: 'bool',
          value: 0,
        },
      ],
    };

    const { onParameterChange } = renderRack(master);

    // Mute must be an explicit, labelled control rather than the header switch.
    const mute = screen.getByRole('switch', { name: 'Mute' });
    expect(mute).toBeInTheDocument();
    expect(screen.queryByRole('switch', { name: 'Master on/off' })).not.toBeInTheDocument();

    fireEvent.click(mute);
    expect(onParameterChange).toHaveBeenLastCalledWith('master', 'muted', 1);
  });

  it('shows its position in the chain when told where it sits', () => {
    const onParameterChange = vi.fn();
    const onEnabledChange = vi.fn();

    render(
      <EffectRack
        effect={overdrive}
        index={4}
        total={10}
        onParameterChange={onParameterChange}
        onEnabledChange={onEnabledChange}
      />,
    );

    expect(screen.getByText('4/10')).toBeInTheDocument();
  });

  it('dims and disables controls when the effect is off', () => {
    renderRack({ ...overdrive, enabled: false });

    expect(screen.getByRole('slider', { name: 'Drive' })).toHaveAttribute('aria-disabled', 'true');
  });
});

describe('EffectRack pins', () => {
  it('shows no pin buttons when the engine does not track pins', () => {
    renderRack(overdrive);

    expect(screen.queryByRole('button', { name: /Sematkan/ })).not.toBeInTheDocument();
  });

  it('pins a control and reports which one', () => {
    const onTogglePin = vi.fn();

    render(
      <EffectRack
        effect={overdrive}
        onParameterChange={vi.fn()}
        onEnabledChange={vi.fn()}
        onTogglePin={onTogglePin}
      />,
    );

    fireEvent.click(screen.getByRole('button', { name: 'Sematkan Drive ke layar Perform' }));

    expect(onTogglePin).toHaveBeenCalledWith('overdrive', 'drivePct');
  });

  it('marks an already-pinned control as pressed, and offers to unpin it', () => {
    render(
      <EffectRack
        effect={overdrive}
        onParameterChange={vi.fn()}
        onEnabledChange={vi.fn()}
        pinnedParams={new Set(['overdrive.drivePct'])}
        onTogglePin={vi.fn()}
      />,
    );

    const pin = screen.getByRole('button', { name: 'Lepas Drive dari layar Perform' });
    expect(pin).toHaveAttribute('aria-pressed', 'true');
  });
});

describe('EffectRack chain order', () => {
  it('shows no move controls when the engine does not support reordering', () => {
    renderRack(overdrive);

    expect(screen.queryByRole('button', { name: /Pindahkan/ })).not.toBeInTheDocument();
  });

  it('moves a stage earlier and later in the chain', () => {
    const onMove = vi.fn();

    render(
      <EffectRack
        effect={overdrive}
        onParameterChange={vi.fn()}
        onEnabledChange={vi.fn()}
        onMove={onMove}
        canMoveUp
        canMoveDown
      />,
    );

    fireEvent.click(screen.getByRole('button', { name: /lebih awal di rantai/ }));
    expect(onMove).toHaveBeenCalledWith('overdrive', -1);

    fireEvent.click(screen.getByRole('button', { name: /lebih akhir di rantai/ }));
    expect(onMove).toHaveBeenCalledWith('overdrive', 1);
  });

  it('disables the arrow that would move it past the end', () => {
    render(
      <EffectRack
        effect={overdrive}
        onParameterChange={vi.fn()}
        onEnabledChange={vi.fn()}
        onMove={vi.fn()}
        canMoveUp={false}
        canMoveDown
      />,
    );

    expect(screen.getByRole('button', { name: /lebih awal di rantai/ })).toBeDisabled();
    expect(screen.getByRole('button', { name: /lebih akhir di rantai/ })).toBeEnabled();
  });

  it('locks a pinned stage and says why rather than just greying it out', () => {
    // The master stage carries the safety limiter, so nothing may follow it.
    // A control that is merely disabled invites the question; this answers it.
    render(
      <EffectRack
        effect={{ ...overdrive, id: 'master', label: 'Master' }}
        onParameterChange={vi.fn()}
        onEnabledChange={vi.fn()}
        onMove={vi.fn()}
        movable={false}
      />,
    );

    expect(screen.queryByRole('button', { name: /Pindahkan/ })).not.toBeInTheDocument();

    const locked = screen.getByLabelText('Posisi terkunci');
    expect(locked).toHaveAttribute('title', expect.stringContaining('limiter pengaman'));
  });
});

describe('EffectRack A/B paths', () => {
  it('offers no path selector when the stage is outside the parallel section', () => {
    // A selector on a stage the split does not reach would do nothing at all.
    renderRack(overdrive);

    expect(screen.queryByRole('button', { name: /jalur A/ })).not.toBeInTheDocument();
  });

  it('shows which path the stage runs on and switches it', () => {
    const onBusChange = vi.fn();

    render(
      <EffectRack
        effect={overdrive}
        onParameterChange={vi.fn()}
        onEnabledChange={vi.fn()}
        bus="A"
        onBusChange={onBusChange}
      />,
    );

    const a = screen.getByRole('button', { name: 'Jalankan Overdrive di jalur A' });
    const b = screen.getByRole('button', { name: 'Jalankan Overdrive di jalur B' });

    expect(a).toHaveAttribute('aria-pressed', 'true');
    expect(b).toHaveAttribute('aria-pressed', 'false');

    fireEvent.click(b);
    expect(onBusChange).toHaveBeenCalledWith('overdrive', 'B');
  });
});

describe('EffectRack split modes', () => {
  const split = (mode: number): EffectDescriptor => ({
    id: 'split',
    label: 'Split',
    description: 'Belah sinyal jadi dua jalur - A dan B',
    enabled: true,
    toggleable: true,
    parameters: [
      {
        id: 'mode',
        label: 'Mode',
        unit: '',
        min: 0,
        max: 2,
        step: 1,
        default: 0,
        type: 'float',
        value: mode,
      },
      {
        id: 'freqHz',
        label: 'Frekuensi',
        unit: 'Hz',
        min: 60,
        max: 2000,
        step: 1,
        default: 250,
        type: 'float',
        value: 250,
      },
    ],
  });

  it('offers all three modes, L/R included', () => {
    renderRack(split(0));

    const select = screen.getByLabelText('Mode') as HTMLSelectElement;
    expect(select.querySelectorAll('option')).toHaveLength(3);

    // The one that makes a two-pickup guitar work: two sources routed, not one
    // signal divided.
    expect(screen.getByRole('option', { name: /L\/R - kanal L ke A/ })).toBeInTheDocument();
  });

  it('keeps the crossover frequency live only in crossover mode', () => {
    const { unmount } = render(
      <EffectRack effect={split(1)} onParameterChange={vi.fn()} onEnabledChange={vi.fn()} />,
    );

    // The knob signals its state through aria-disabled and the tab order, not
    // through a `disabled` attribute -- it is a div with role="slider".
    expect(screen.getByRole('slider', { name: /Frekuensi/ })).toHaveAttribute(
      'aria-disabled',
      'false',
    );
    unmount();

    // In L/R mode the crossover filter is not in the path at all, so a live
    // frequency knob would be a control that does nothing.
    render(
      <EffectRack effect={split(2)} onParameterChange={vi.fn()} onEnabledChange={vi.fn()} />,
    );

    const knob = screen.getByRole('slider', { name: /Frekuensi/ });
    expect(knob).toHaveAttribute('aria-disabled', 'true');
    expect(knob).toHaveAttribute('tabindex', '-1');
  });

  it('writes the chosen mode straight through as its index', () => {
    const onParameterChange = vi.fn();

    render(
      <EffectRack
        effect={split(0)}
        onParameterChange={onParameterChange}
        onEnabledChange={vi.fn()}
      />,
    );

    fireEvent.change(screen.getByLabelText('Mode'), { target: { value: '2' } });
    expect(onParameterChange).toHaveBeenCalledWith('split', 'mode', 2);
  });
});

describe('EffectRack board removal', () => {
  it('offers no remove button when the engine has no board', () => {
    renderRack(overdrive);

    expect(screen.queryByRole('button', { name: /Buang/ })).not.toBeInTheDocument();
  });

  it('takes the stage off the board when asked', () => {
    const onRemove = vi.fn();

    render(
      <EffectRack
        effect={overdrive}
        onParameterChange={vi.fn()}
        onEnabledChange={vi.fn()}
        onRemove={onRemove}
      />,
    );

    fireEvent.click(screen.getByRole('button', { name: 'Buang Overdrive dari board' }));
    expect(onRemove).toHaveBeenCalledWith('overdrive');
  });

  it('keeps removal separate from the on/off switch', () => {
    const onRemove = vi.fn();
    const onEnabledChange = vi.fn();

    render(
      <EffectRack
        effect={overdrive}
        onParameterChange={vi.fn()}
        onEnabledChange={onEnabledChange}
        onRemove={onRemove}
      />,
    );

    // Bypassing keeps the stage in the chain so its tail can decay; removing it
    // takes it out entirely. Two gestures, and neither may stand in for the other.
    fireEvent.click(screen.getByRole('button', { name: 'Buang Overdrive dari board' }));

    expect(onRemove).toHaveBeenCalled();
    expect(onEnabledChange).not.toHaveBeenCalled();
  });
});

describe('EffectRack surfaces', () => {
  it('marks the card with its block type, not its instance id', () => {
    const { container } = render(
      <EffectRack
        effect={{ ...overdrive, id: 'overdrive2', label: 'Overdrive 2' }}
        onParameterChange={vi.fn()}
        onEnabledChange={vi.fn()}
      />,
    );

    // Keyed by type so a second instance gets the same surface as the first.
    expect(container.querySelector('.rack--overdrive')).toBeInTheDocument();
    expect(container.querySelector('.rack--overdrive2')).not.toBeInTheDocument();
  });

  it('gives the cabinet and the amp their own surface', () => {
    // The only two blocks that get real artwork, because both are single and
    // neither changes shape with any parameter.
    const { container: cab } = render(
      <EffectRack effect={cabinet} onParameterChange={vi.fn()} onEnabledChange={vi.fn()} />,
    );
    expect(cab.querySelector('.rack--cabinet')).toBeInTheDocument();

    const { container: amp } = render(
      <EffectRack
        effect={{ ...cabinet, id: 'nam', label: 'Amp (NAM)' }}
        onParameterChange={vi.fn()}
        onEnabledChange={vi.fn()}
      />,
    );
    expect(amp.querySelector('.rack--nam')).toBeInTheDocument();
  });
});

describe('EffectRack control shapes', () => {
  const mixer: EffectDescriptor = {
    id: 'mixer',
    label: 'Mixer',
    description: 'Gabungkan jalur A dan B',
    enabled: true,
    toggleable: false,
    parameters: [
      { id: 'levelA', label: 'Level A', unit: '', min: 0, max: 2, step: 0.01, default: 1, type: 'float', value: 1 },
      { id: 'panA', label: 'Pan A', unit: '', min: -1, max: 1, step: 0.01, default: 0, type: 'float', value: 0 },
      { id: 'levelB', label: 'Level B', unit: '', min: 0, max: 2, step: 0.01, default: 1, type: 'float', value: 1 },
    ],
  };

  it('gives the Mixer levels faders and its pan a knob', () => {
    const { container } = render(
      <EffectRack effect={mixer} onParameterChange={vi.fn()} onEnabledChange={vi.fn()} />,
    );

    // Two heights are far easier to compare than two rotations, and comparing
    // is the actual task on the mixer. Pan stays a knob because it is bipolar
    // around a centre, which is what every console does too.
    expect(container.querySelectorAll('.fader')).toHaveLength(2);
    expect(screen.getByRole('slider', { name: 'Level A' })).toBeInTheDocument();
    expect(container.querySelector('.knob')).toBeInTheDocument();
  });

  it('leaves every other block on knobs', () => {
    const { container } = render(
      <EffectRack effect={overdrive} onParameterChange={vi.fn()} onEnabledChange={vi.fn()} />,
    );

    // The hardware is a knob for every one of these, which is the whole reason.
    expect(container.querySelector('.fader')).not.toBeInTheDocument();
  });

  it('draws the first control of a block larger than the rest', () => {
    const { container } = render(
      <EffectRack effect={overdrive} onParameterChange={vi.fn()} onEnabledChange={vi.fn()} />,
    );

    // A pedal has one big knob and several small ones, and the hierarchy reads
    // before the labels do. Derived from position, not a per-effect list.
    const dials = container.querySelectorAll<HTMLElement>('.knob__dial');
    expect(dials[0].style.width).toBe('92px');
    expect(dials[1].style.width).toBe('72px');
  });
});

describe('EffectRack instance layouts', () => {
  it('gives a second overdrive instance the same voicing layout as the first', () => {
    // Regression: this check was against the raw id, so every instance past the
    // first showed the union of all twelve voicings' controls instead of the
    // selected voicing's. No error, just the wrong knobs.
    const bigMuff = { ...overdriveAs(11), id: 'overdrive2', label: 'Overdrive 2' };

    render(
      <EffectRack effect={bigMuff} onParameterChange={vi.fn()} onEnabledChange={vi.fn()} />,
    );

    expect(screen.getByRole('slider', { name: 'Sustain' })).toBeInTheDocument();
    expect(screen.getByRole('slider', { name: 'Tone' })).toBeInTheDocument();
    expect(screen.getByRole('slider', { name: 'Volume' })).toBeInTheDocument();

    // Controls belonging to other voicings must not be on the card.
    expect(screen.queryByRole('slider', { name: 'Asymmetry' })).not.toBeInTheDocument();
    expect(screen.queryByRole('slider', { name: 'Bass' })).not.toBeInTheDocument();
  });
});

describe('EffectRack in English', () => {
  const inEnglish = (effect: EffectDescriptor) => {
    window.localStorage.setItem('milodikfx.language', 'en');

    render(
      <LanguageProvider>
        <EffectRack effect={effect} onParameterChange={vi.fn()} onEnabledChange={vi.fn()} />
      </LanguageProvider>,
    );
  };

  afterEach(() => window.localStorage.clear());

  it('describes the block in the chosen language rather than the engine’s', () => {
    // The engine sends one description, in one language, and a DAW automation
    // lane is stuck with it. What the UI draws comes from the dictionary.
    inEnglish(overdrive);

    expect(screen.getByText(/pick the pedal voicing/i)).toBeInTheDocument();
    expect(screen.queryByText(/pilih voicing pedalnya/i)).not.toBeInTheDocument();
  });

  it('words the choices in English too', () => {
    inEnglish({ ...input, id: 'input' });

    expect(screen.getByRole('option', { name: 'Mono - sum of both' })).toBeInTheDocument();
  });

  it('leaves the voicing names alone — they are products, not words', () => {
    inEnglish(overdrive);

    expect(screen.getByRole('option', { name: 'Tube Screamer' })).toBeInTheDocument();
    expect(screen.getByRole('option', { name: 'Marshall-in-a-Box' })).toBeInTheDocument();
  });
});
