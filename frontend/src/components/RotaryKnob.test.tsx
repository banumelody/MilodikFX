import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { vi } from 'vitest';
import RotaryKnob from './RotaryKnob';

describe('RotaryKnob', () => {
  it('renders and exposes slider role and aria attributes', () => {
    const onChange = vi.fn();
    render(<RotaryKnob value={50} onChange={onChange} min={0} max={100} step={1} size={72} />);
    const slider = screen.getByRole('slider');
    expect(slider).toBeInTheDocument();
    expect(slider).toHaveAttribute('aria-valuemin', '0');
    expect(slider).toHaveAttribute('aria-valuemax', '100');
    expect(slider).toHaveAttribute('aria-valuenow', '50');
  });

  it('responds to keyboard events (ArrowUp/ArrowDown) by calling onChange', async () => {
    const onChange = vi.fn();
    render(<RotaryKnob value={10} onChange={onChange} min={0} max={100} step={1} size={72} />);
    const user = userEvent.setup();
    const slider = screen.getByRole('slider');

    // focus then press ArrowUp
    slider.focus();
    await user.keyboard('{ArrowUp}');
    expect(onChange).toHaveBeenCalled();

    // press ArrowDown
    await user.keyboard('{ArrowDown}');
    expect(onChange).toHaveBeenCalledTimes(2);
  });
});
