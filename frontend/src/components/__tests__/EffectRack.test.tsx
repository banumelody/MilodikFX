import { fireEvent, render, screen } from '@testing-library/react';
import { describe, expect, it, vi } from 'vitest';

import { EffectRack } from '../EffectRack';
import type { EffectDescriptor } from '../../services/api';

const overdrive: EffectDescriptor = {
  id: 'overdrive',
  label: 'Overdrive',
  description: 'Cubic soft clipper',
  enabled: true,
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

  it('has no on/off switch for the input stage, which cannot be bypassed', () => {
    renderRack(input);
    expect(screen.queryByRole('switch')).not.toBeInTheDocument();
  });

  it('dims and disables controls when the effect is off', () => {
    renderRack({ ...overdrive, enabled: false });

    expect(screen.getByRole('slider', { name: 'Drive' })).toHaveAttribute('aria-disabled', 'true');
  });
});
