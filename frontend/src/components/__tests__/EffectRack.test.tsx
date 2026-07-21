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

describe('EffectRack', () => {
  it('renders every parameter the engine advertises', () => {
    renderRack(overdrive);

    expect(screen.getByRole('slider', { name: 'Drive' })).toBeInTheDocument();
    expect(screen.getByRole('slider', { name: 'Asymmetry' })).toBeInTheDocument();
    expect(screen.getByRole('combobox')).toBeInTheDocument();
    expect(screen.getByRole('switch', { name: 'Overdrive on/off' })).toBeInTheDocument();
  });

  it('reports knob movement with the effect and parameter id', () => {
    const { onParameterChange } = renderRack(overdrive);

    fireEvent.keyDown(screen.getByRole('slider', { name: 'Drive' }), { key: 'ArrowUp' });

    expect(onParameterChange).toHaveBeenLastCalledWith('overdrive', 'drivePct', 25.5);
  });

  it('renders an enum parameter as named choices, not a raw number', () => {
    const { onParameterChange } = renderRack(overdrive);

    const select = screen.getByRole('combobox');
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
