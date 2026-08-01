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

describe('Knob material', () => {
  it('carries a cap layer for the material treatment', () => {
    const { container } = render(<Knob value={50} onChange={vi.fn()} label="Drive" />);

    // A CSS layer rather than SVG <defs>: there are twenty-six effects' worth
    // of knobs on screen and a gradient id per instance would be waste.
    expect(container.querySelector('.knob__cap')).toBeInTheDocument();
  });

  it('drops the treatment when asked to be plain', () => {
    const { container } = render(
      <Knob value={50} onChange={vi.fn()} label="Drive" plain />,
    );

    // Perform view has to be read from two metres in bad light. It uses this
    // same component, so the opt-out has to be explicit -- and asserted, or it
    // is one refactor away from silently inheriting the texture.
    expect(container.querySelector('.knob--plain')).toBeInTheDocument();
  });

  it('is not plain unless asked', () => {
    const { container } = render(<Knob value={50} onChange={vi.fn()} label="Drive" />);

    expect(container.querySelector('.knob--plain')).not.toBeInTheDocument();
  });

  it('keeps every accessible attribute through the restyle', () => {
    // This is paint, not a change of control. If any of these moved, the
    // keyboard and screen-reader paths would have been traded for a highlight.
    render(<Knob value={25} min={0} max={100} onChange={vi.fn()} label="Drive" unit="%" />);

    const dial = screen.getByRole('slider', { name: 'Drive' });
    expect(dial).toHaveAttribute('aria-valuemin', '0');
    expect(dial).toHaveAttribute('aria-valuemax', '100');
    expect(dial).toHaveAttribute('aria-valuenow', '25');
    expect(dial).toHaveAttribute('aria-valuetext', '25 %');
    expect(dial).toHaveAttribute('aria-disabled', 'false');
    expect(dial).toHaveAttribute('tabindex', '0');
  });
});

/** Mirrors Knob's own constant; the assertions below are fractions of it. */
const DRAG_RANGE_PX = 220;

describe('Knob scale', () => {
  it('prints scale marks around the dial', () => {
    const { container } = render(<Knob value={50} onChange={vi.fn()} label="Drive" />);

    // A hardware knob is absolute -- its pointer position is the value. This
    // one is a relative drag, so the drawing is the only absolute reference
    // there is, and a printed panel gives that away for free.
    expect(container.querySelectorAll('.knob__tick')).toHaveLength(9);
  });

  it('marks the centre only when the range crosses zero', () => {
    const { container: bipolar } = render(
      <Knob value={0} min={-12} max={12} onChange={vi.fn()} label="Bass" />,
    );
    expect(bipolar.querySelector('.knob__centre')).toBeInTheDocument();

    const { container: unipolar } = render(
      <Knob value={50} min={0} max={100} onChange={vi.fn()} label="Drive" />,
    );
    // Derived from the range, not a list of parameter ids -- so a new bipolar
    // parameter is covered without anyone remembering to add it.
    expect(unipolar.querySelector('.knob__centre')).not.toBeInTheDocument();
  });
});

describe('Knob logarithmic travel', () => {
  const attack = (onChange: () => void) =>
    render(
      <Knob
        value={0.1}
        min={0.1}
        max={200}
        step={0.1}
        onChange={onChange}
        label="Attack"
        logScale
      />,
    );

  it('puts the geometric midpoint at half the travel', () => {
    const onChange = vi.fn();
    const { container } = attack(onChange);

    const dial = container.querySelector('[role="slider"]')!;
    fireEvent.pointerDown(dial, { clientX: 0, clientY: 0 });
    fireEvent.pointerMove(window, { clientX: 0, clientY: -DRAG_RANGE_PX / 2 });

    // sqrt(0.1 * 200) = 4.47 ms. Linearly the same gesture lands on 100 ms,
    // which is the whole complaint: everything usable was crammed into the
    // first 2.5% of the travel.
    const landed = onChange.mock.calls.at(-1)![0];
    expect(landed).toBeGreaterThan(4);
    expect(landed).toBeLessThan(5);
  });

  it('still reaches both ends exactly', () => {
    const onChange = vi.fn();
    const { container } = attack(onChange);

    const dial = container.querySelector('[role="slider"]')!;
    fireEvent.pointerDown(dial, { clientX: 0, clientY: 0 });
    fireEvent.pointerMove(window, { clientX: 0, clientY: -5000 });
    expect(onChange.mock.calls.at(-1)![0]).toBe(200);

    fireEvent.pointerMove(window, { clientX: 0, clientY: 5000 });
    expect(onChange.mock.calls.at(-1)![0]).toBeCloseTo(0.1, 5);
  });

  it('always moves on an arrow key, even where a step rounds away', () => {
    // At the bottom of a 2000:1 range one normalised step is smaller than the
    // parameter's own step, so quantising lands back where it started. A key
    // press that does nothing reads as a broken control.
    const onChange = vi.fn();
    const { container } = attack(onChange);

    fireEvent.keyDown(container.querySelector('[role="slider"]')!, { key: 'ArrowUp' });

    expect(onChange).toHaveBeenCalled();
    expect(onChange.mock.calls.at(-1)![0]).toBeGreaterThan(0.1);
  });

  it('leaves a linear parameter untouched', () => {
    const onChange = vi.fn();
    const { container } = render(
      <Knob value={50} min={0} max={100} step={1} onChange={onChange} label="Drive" />,
    );

    const dial = container.querySelector('[role="slider"]')!;
    fireEvent.pointerDown(dial, { clientX: 0, clientY: 0 });
    fireEvent.pointerMove(window, { clientX: 0, clientY: -DRAG_RANGE_PX / 4 });

    // A quarter of the travel from the middle of a linear range is 75.
    expect(onChange.mock.calls.at(-1)![0]).toBe(75);
  });
});
