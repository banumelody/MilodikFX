import { fireEvent, render, screen } from '@testing-library/react';
import { describe, expect, it, vi } from 'vitest';

import { Knob } from '../Knob';

function renderKnob(overrides: Partial<React.ComponentProps<typeof Knob>> = {}) {
  const onChange = vi.fn();

  const utils = render(
    <Knob
      value={50}
      min={0}
      max={100}
      step={1}
      defaultValue={0}
      label="Drive"
      unit="%"
      onChange={onChange}
      {...overrides}
    />,
  );

  return { ...utils, onChange, dial: screen.getByRole('slider') };
}

describe('Knob', () => {
  it('exposes its range and current value to assistive tech', () => {
    const { dial } = renderKnob();

    expect(dial).toHaveAttribute('aria-valuemin', '0');
    expect(dial).toHaveAttribute('aria-valuemax', '100');
    expect(dial).toHaveAttribute('aria-valuenow', '50');
    expect(dial).toHaveAttribute('aria-valuetext', '50 %');
    expect(dial).toHaveAttribute('aria-label', 'Drive');
  });

  it('does not change value on a click without movement', () => {
    // The previous knob mapped the pointer's absolute angle, so a single click
    // anywhere on the dial jumped the value to whatever that position meant.
    const { dial, onChange } = renderKnob();

    fireEvent.pointerDown(dial, { clientX: 5, clientY: 5 });
    fireEvent.pointerUp(window);

    expect(onChange).not.toHaveBeenCalled();
  });

  it('increases the value when dragged upwards, relative to the press point', () => {
    const { dial, onChange } = renderKnob();

    fireEvent.pointerDown(dial, { clientX: 40, clientY: 200 });
    fireEvent.pointerMove(window, { clientX: 40, clientY: 90 });

    expect(onChange).toHaveBeenCalled();

    const next = onChange.mock.calls.at(-1)![0];
    expect(next).toBeGreaterThan(50);
    expect(next).toBeLessThanOrEqual(100);
  });

  it('decreases the value when dragged downwards', () => {
    const { dial, onChange } = renderKnob();

    fireEvent.pointerDown(dial, { clientX: 40, clientY: 100 });
    fireEvent.pointerMove(window, { clientX: 40, clientY: 200 });

    const next = onChange.mock.calls.at(-1)![0];
    expect(next).toBeLessThan(50);
    expect(next).toBeGreaterThanOrEqual(0);
  });

  it('moves in smaller increments while shift is held', () => {
    const { dial, onChange } = renderKnob();

    fireEvent.pointerDown(dial, { clientX: 40, clientY: 200 });
    fireEvent.pointerMove(window, { clientX: 40, clientY: 100, shiftKey: true });
    const fine = onChange.mock.calls.at(-1)![0] - 50;

    fireEvent.pointerUp(window);
    onChange.mockClear();

    fireEvent.pointerDown(dial, { clientX: 40, clientY: 200 });
    fireEvent.pointerMove(window, { clientX: 40, clientY: 100 });
    const coarse = onChange.mock.calls.at(-1)![0] - 50;

    expect(fine).toBeGreaterThan(0);
    expect(fine).toBeLessThan(coarse);
  });

  it('never leaves the declared range', () => {
    const { dial, onChange } = renderKnob();

    fireEvent.pointerDown(dial, { clientX: 40, clientY: 500 });
    fireEvent.pointerMove(window, { clientX: 40, clientY: -5000 });
    expect(onChange.mock.calls.at(-1)![0]).toBe(100);

    fireEvent.pointerUp(window);
    fireEvent.pointerDown(dial, { clientX: 40, clientY: 0 });
    fireEvent.pointerMove(window, { clientX: 40, clientY: 5000 });
    expect(onChange.mock.calls.at(-1)![0]).toBe(0);
  });

  it('supports the keyboard', () => {
    const { dial, onChange } = renderKnob();

    fireEvent.keyDown(dial, { key: 'ArrowUp' });
    expect(onChange).toHaveBeenLastCalledWith(51);

    fireEvent.keyDown(dial, { key: 'ArrowDown' });
    expect(onChange).toHaveBeenLastCalledWith(49);

    fireEvent.keyDown(dial, { key: 'PageUp' });
    expect(onChange).toHaveBeenLastCalledWith(60);

    fireEvent.keyDown(dial, { key: 'Home' });
    expect(onChange).toHaveBeenLastCalledWith(0);

    fireEvent.keyDown(dial, { key: 'End' });
    expect(onChange).toHaveBeenLastCalledWith(100);
  });

  it('supports the wheel and a double-click reset', () => {
    const { dial, onChange } = renderKnob();

    fireEvent.wheel(dial, { deltaY: -100 });
    expect(onChange.mock.calls.at(-1)![0]).toBeGreaterThan(50);

    fireEvent.wheel(dial, { deltaY: 100 });
    expect(onChange.mock.calls.at(-1)![0]).toBeLessThan(50);

    fireEvent.doubleClick(dial);
    expect(onChange).toHaveBeenLastCalledWith(0);
  });

  it('quantises to the step', () => {
    const { dial, onChange } = renderKnob({ value: 0, min: 0, max: 10, step: 0.5 });

    fireEvent.keyDown(dial, { key: 'ArrowUp' });
    expect(onChange).toHaveBeenLastCalledWith(0.5);
  });

  it('ignores input when disabled', () => {
    const { dial, onChange } = renderKnob({ disabled: true });

    fireEvent.pointerDown(dial, { clientX: 40, clientY: 200 });
    fireEvent.pointerMove(window, { clientX: 40, clientY: 100 });
    fireEvent.keyDown(dial, { key: 'ArrowUp' });
    fireEvent.wheel(dial, { deltaY: -100 });

    expect(onChange).not.toHaveBeenCalled();
    expect(dial).toHaveAttribute('tabindex', '-1');
  });
});
