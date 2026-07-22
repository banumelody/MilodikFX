import { fireEvent, render, screen } from '@testing-library/react';
import { describe, expect, it, vi } from 'vitest';

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

  render(
    <EffectRack
      effect={effect}
      onParameterChange={onParameterChange}
      onEnabledChange={onEnabledChange}
    />,
  );

  return { onParameterChange, onEnabledChange };
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
