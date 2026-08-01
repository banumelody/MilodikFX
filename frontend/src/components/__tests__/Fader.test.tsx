import { fireEvent, render, screen } from '@testing-library/react';
import { describe, expect, it, vi } from 'vitest';

import { Fader } from '../Fader';

/** Mirrors Fader's own constant, which it shares with Knob. */
const DRAG_RANGE_PX = 220;

function renderFader(overrides: Partial<React.ComponentProps<typeof Fader>> = {}) {
  const onChange = vi.fn();

  render(
    <Fader value={1} min={0} max={2} step={0.01} label="Level A" onChange={onChange} {...overrides} />,
  );

  return { onChange, track: screen.getByRole('slider', { name: 'Level A' }) };
}

describe('Fader', () => {
  it('is a slider with the value it was given', () => {
    const { track } = renderFader();

    expect(track).toHaveAttribute('aria-valuemin', '0');
    expect(track).toHaveAttribute('aria-valuemax', '2');
    expect(track).toHaveAttribute('aria-valuenow', '1');
  });

  it('drags relative to the press point, up for more', () => {
    // The same gesture as Knob, on purpose: a fader that behaved differently
    // from the knob beside it would be a second thing to remember for no gain.
    const { onChange, track } = renderFader();

    fireEvent.pointerDown(track, { clientY: 0 });
    fireEvent.pointerMove(window, { clientY: -DRAG_RANGE_PX / 2 });

    // Half the travel of a 0..2 range from the middle is 2.
    expect(onChange.mock.calls.at(-1)![0]).toBe(2);
  });

  it('goes down when dragged down', () => {
    const { onChange, track } = renderFader();

    fireEvent.pointerDown(track, { clientY: 0 });
    fireEvent.pointerMove(window, { clientY: DRAG_RANGE_PX / 4 });

    expect(onChange.mock.calls.at(-1)![0]).toBeCloseTo(0.5, 5);
  });

  it('slows down with shift held', () => {
    const { onChange, track } = renderFader();

    fireEvent.pointerDown(track, { clientY: 0 });
    fireEvent.pointerMove(window, { clientY: -DRAG_RANGE_PX / 2, shiftKey: true });

    // FINE_FACTOR is 0.2, so the same gesture covers a fifth of the distance.
    expect(onChange.mock.calls.at(-1)![0]).toBeCloseTo(1.2, 5);
  });

  it('clamps at both ends', () => {
    const { onChange, track } = renderFader();

    fireEvent.pointerDown(track, { clientY: 0 });
    fireEvent.pointerMove(window, { clientY: -5000 });
    expect(onChange.mock.calls.at(-1)![0]).toBe(2);

    fireEvent.pointerMove(window, { clientY: 5000 });
    expect(onChange.mock.calls.at(-1)![0]).toBe(0);
  });

  it('supports the same keyboard set as the knob', () => {
    const { onChange, track } = renderFader();

    fireEvent.keyDown(track, { key: 'ArrowUp' });
    expect(onChange).toHaveBeenLastCalledWith(1.01);

    fireEvent.keyDown(track, { key: 'PageDown' });
    expect(onChange).toHaveBeenLastCalledWith(0.8);

    fireEvent.keyDown(track, { key: 'Home' });
    expect(onChange).toHaveBeenLastCalledWith(0);

    fireEvent.keyDown(track, { key: 'End' });
    expect(onChange).toHaveBeenLastCalledWith(2);
  });

  it('returns to its default on a double click', () => {
    const { onChange, track } = renderFader({ defaultValue: 1.5 });

    fireEvent.doubleClick(track);
    expect(onChange).toHaveBeenLastCalledWith(1.5);
  });

  it('does nothing at all while disabled', () => {
    const { onChange, track } = renderFader({ disabled: true });

    fireEvent.pointerDown(track, { clientY: 0 });
    fireEvent.pointerMove(window, { clientY: -100 });
    fireEvent.keyDown(track, { key: 'ArrowUp' });

    expect(onChange).not.toHaveBeenCalled();
    expect(track).toHaveAttribute('tabindex', '-1');
  });

  it('carries scale marks, like the knob', () => {
    const { container } = render(
      <Fader value={1} min={0} max={2} label="Level A" onChange={vi.fn()} />,
    );

    expect(container.querySelectorAll('.fader__scale i')).toHaveLength(5);
  });
});
