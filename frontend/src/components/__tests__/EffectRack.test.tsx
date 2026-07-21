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
      id: 'oversampling',
      label: 'Oversampling',
      unit: '',
      min: 0,
      max: 1,
      step: 1,
      default: 1,
      type: 'bool',
      value: 1,
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
    expect(screen.getByRole('switch', { name: 'Oversampling' })).toBeInTheDocument();
    expect(screen.getByRole('switch', { name: 'Overdrive on/off' })).toBeInTheDocument();
  });

  it('reports knob movement with the effect and parameter id', () => {
    const { onParameterChange } = renderRack(overdrive);

    fireEvent.keyDown(screen.getByRole('slider', { name: 'Drive' }), { key: 'ArrowUp' });

    expect(onParameterChange).toHaveBeenLastCalledWith('overdrive', 'drivePct', 25.5);
  });

  it('sends booleans as 0 or 1', () => {
    const { onParameterChange } = renderRack(overdrive);

    fireEvent.click(screen.getByRole('switch', { name: 'Oversampling' }));

    expect(onParameterChange).toHaveBeenLastCalledWith('overdrive', 'oversampling', 0);
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

  it('dims and disables controls when the effect is off', () => {
    renderRack({ ...overdrive, enabled: false });

    expect(screen.getByRole('slider', { name: 'Drive' })).toHaveAttribute('aria-disabled', 'true');
  });
});
