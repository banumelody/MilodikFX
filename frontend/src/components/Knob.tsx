import { memo, useCallback, useEffect, useRef, useState } from 'react';

export interface KnobProps {
  value: number;
  onChange: (value: number) => void;
  onCommit?: (value: number) => void;
  min?: number;
  max?: number;
  step?: number;
  defaultValue?: number;
  size?: number;
  /**
   * Drops the material treatment.
   *
   * Perform view has to be read from two metres in bad light, and a specular
   * highlight costs contrast there for nothing. It uses this same component, so
   * the opt-out is a prop rather than a hope.
   */
  plain?: boolean;
  /**
   * Travel is logarithmic rather than linear.
   *
   * A gesture hint only -- the value, the API and the stored preset are all
   * unchanged. Time and frequency are perceived logarithmically, and mapping
   * them linearly to drag distance buries the useful region: a compressor
   * attack of 0.1-200 ms puts everything from 0.1 to 5 ms in the first 2.5 %
   * of the travel.
   */
  logScale?: boolean;
  label?: string;
  unit?: string;
  accent?: string;
  disabled?: boolean;
  /** Formats the readout; defaults to a step-appropriate number of decimals. */
  format?: (value: number) => string;
}

const START_ANGLE = -135;
const ANGLE_RANGE = 270;

/** Pixels of vertical travel that span the whole range. */
const DRAG_RANGE_PX = 220;

/** Multiplier applied while shift is held. */
const FINE_FACTOR = 0.2;

function polar(cx: number, cy: number, radius: number, degrees: number) {
  const radians = ((degrees - 90) * Math.PI) / 180;
  return { x: cx + radius * Math.cos(radians), y: cy + radius * Math.sin(radians) };
}

function arcPath(cx: number, cy: number, radius: number, from: number, to: number) {
  const start = polar(cx, cy, radius, from);
  const end = polar(cx, cy, radius, to);
  const largeArc = Math.abs(to - from) > 180 ? 1 : 0;
  return `M ${start.x} ${start.y} A ${radius} ${radius} 0 ${largeArc} 1 ${end.x} ${end.y}`;
}

function decimalsForStep(step: number) {
  if (step >= 1) return 0;
  if (step >= 0.1) return 1;
  return 2;
}

function KnobBase({
  value,
  onChange,
  onCommit,
  min = 0,
  max = 100,
  step = 1,
  defaultValue,
  size = 76,
  plain = false,
  logScale = false,
  label,
  unit = '',
  accent = '#4da3ff',
  disabled = false,
  format,
}: KnobProps) {
  const [dragging, setDragging] = useState(false);
  const drag = useRef<{ startY: number; startValue: number } | null>(null);

  const span = max - min || 1;

  // Position along the travel, 0..1. Logarithmic when asked for, which needs a
  // positive minimum -- a log scale has no zero, so a range that reaches it
  // falls back to linear rather than producing NaN.
  const curved = logScale && min > 0 && max > min;

  const toNorm = useCallback(
    (v: number) => {
      const clampedValue = Math.min(max, Math.max(min, v));

      if (!curved) return (clampedValue - min) / span;

      return Math.log(clampedValue / min) / Math.log(max / min);
    },
    [curved, min, max, span],
  );

  const fromNorm = useCallback(
    (t: number) => {
      const clampedNorm = Math.min(1, Math.max(0, t));

      if (!curved) return min + clampedNorm * span;

      return min * Math.pow(max / min, clampedNorm);
    },
    [curved, min, max, span],
  );

  const quantise = useCallback(
    (raw: number) => {
      const stepped = step > 0 ? Math.round((raw - min) / step) * step + min : raw;
      return Math.min(max, Math.max(min, stepped));
    },
    [min, max, step],
  );

  // A vertical drag, relative to where the pointer went down. The old knob
  // mapped the pointer's absolute angle instead, so a click anywhere on the
  // dial jumped the value straight to whatever that position meant.
  const applyDelta = useCallback(
    (deltaPx: number, fine: boolean) => {
      const state = drag.current;
      if (!state) return;

      // Through the curve, so the same pixel always moves the same fraction of
      // the travel whatever the range does underneath.
      const scale = (fine ? FINE_FACTOR : 1) / DRAG_RANGE_PX;
      const next = fromNorm(toNorm(state.startValue) + deltaPx * scale);

      onChange(quantise(next));
    },
    [onChange, quantise, toNorm, fromNorm],
  );

  /**
   * One nudge in the direction given, for the wheel and the arrow keys.
   *
   * A curved parameter steps in *normalised* space: one `step` at the bottom of
   * a 2000:1 range is a huge jump and at the top it is invisible. Quantising
   * afterwards can land back on the value it started from, so a nudge that did
   * not move is pushed one `step` instead -- a key press that does nothing
   * reads as a broken control.
   */
  const nudge = useCallback(
    (direction: number, size: 'fine' | 'wheel' | 'coarse') => {
      // Three distinct magnitudes, and they were three before this: an arrow is
      // one step, the wheel is five, PageUp is a tenth of the range. Collapsing
      // them turned PageUp into a nudge.
      if (!curved) {
        const amount = size === 'coarse' ? span / 10 : size === 'wheel' ? step * 5 : step;
        return quantise(value + amount * direction);
      }

      const fraction = size === 'coarse' ? 0.1 : size === 'wheel' ? 0.02 : 0.005;
      const moved = quantise(fromNorm(toNorm(value) + direction * fraction));

      return moved === value ? quantise(value + step * direction) : moved;
    },
    [curved, quantise, value, step, span, toNorm, fromNorm],
  );

  useEffect(() => {
    if (!dragging) return;

    const onMove = (event: PointerEvent) => {
      const state = drag.current;
      if (!state) return;
      applyDelta(state.startY - event.clientY, event.shiftKey);
    };

    const onUp = () => {
      setDragging(false);
      drag.current = null;
      onCommit?.(value);
    };

    window.addEventListener('pointermove', onMove);
    window.addEventListener('pointerup', onUp);
    window.addEventListener('pointercancel', onUp);

    return () => {
      window.removeEventListener('pointermove', onMove);
      window.removeEventListener('pointerup', onUp);
      window.removeEventListener('pointercancel', onUp);
    };
  }, [dragging, applyDelta, onCommit, value]);

  const onPointerDown = (event: React.PointerEvent) => {
    if (disabled) return;
    event.preventDefault();
    drag.current = { startY: event.clientY, startValue: value };
    setDragging(true);
  };

  const onWheel = (event: React.WheelEvent) => {
    if (disabled) return;
    const direction = event.deltaY < 0 ? 1 : -1;
    const next = nudge(direction, event.shiftKey ? 'fine' : 'wheel');
    onChange(next);
    onCommit?.(next);
  };

  const onDoubleClick = () => {
    if (disabled || defaultValue === undefined) return;
    const reset = quantise(defaultValue);
    onChange(reset);
    onCommit?.(reset);
  };

  const onKeyDown = (event: React.KeyboardEvent) => {
    if (disabled) return;

    let next: number | null = null;

    // Through `nudge`, so the keyboard follows the same curve the drag does.
    switch (event.key) {
      case 'ArrowUp':
      case 'ArrowRight':
        next = nudge(1, 'fine');
        break;
      case 'ArrowDown':
      case 'ArrowLeft':
        next = nudge(-1, 'fine');
        break;
      case 'PageUp':
        next = nudge(1, 'coarse');
        break;
      case 'PageDown':
        next = nudge(-1, 'coarse');
        break;
      case 'Home':
        next = min;
        break;
      case 'End':
        next = max;
        break;
      default:
        return;
    }

    event.preventDefault();
    const quantised = quantise(next);
    onChange(quantised);
    onCommit?.(quantised);
  };

  // The drawing follows the curve too, or the pointer would sit somewhere the
  // drag never puts it.
  const ratio = toNorm(value);
  const angle = START_ANGLE + ratio * ANGLE_RANGE;

  const centre = size / 2;
  const radius = centre - 7;
  const text = format ? format(value) : value.toFixed(decimalsForStep(step));
  const readout = unit ? `${text} ${unit}` : text;

  /**
   * Scale marks around the dial.
   *
   * The most universal thing a hardware knob has, and the one this did not.
   * It matters especially here: a hardware knob is *absolute* -- its pointer
   * position is the value, always -- while this one is a relative drag, so the
   * drawing is the only absolute reference there is. A printed panel gives that
   * away for free.
   *
   * Nine marks, so eighths of the travel read at a glance without becoming a
   * texture.
   */
  const ticks = Array.from({ length: 9 }, (_, i) => {
    const at = START_ANGLE + (i / 8) * ANGLE_RANGE;
    return { from: polar(centre, centre, radius + 3, at), to: polar(centre, centre, radius + 6, at) };
  });

  /**
   * The centre mark, for a parameter that runs either side of zero.
   *
   * Derived from the range rather than a list of parameter ids: any range that
   * crosses zero gets one, so pan, the tone stack and asymmetry are covered and
   * a new bipolar parameter is too, without anyone remembering to add it.
   */
  const bipolar = min < 0 && max > 0;
  const centreAngle = START_ANGLE + toNorm(0) * ANGLE_RANGE;
  const centreMark = {
    from: polar(centre, centre, radius + 2, centreAngle),
    to: polar(centre, centre, radius + 8, centreAngle),
  };

  const indicator = polar(centre, centre, radius - 11, angle);
  const indicatorInner = polar(centre, centre, radius - 21, angle);

  return (
    <div
      className={`knob${disabled ? ' knob--disabled' : ''}${plain ? ' knob--plain' : ''}`}
    >
      <div
        className={`knob__dial${dragging ? ' knob__dial--active' : ''}`}
        role="slider"
        aria-label={label}
        aria-valuemin={min}
        aria-valuemax={max}
        aria-valuenow={Number(text)}
        aria-valuetext={readout}
        aria-disabled={disabled}
        tabIndex={disabled ? -1 : 0}
        onPointerDown={onPointerDown}
        onWheel={onWheel}
        onDoubleClick={onDoubleClick}
        onKeyDown={onKeyDown}
        style={{ width: size, height: size, touchAction: 'none' }}
      >
        {/* The cap, as a CSS layer behind the SVG. Doing it in CSS rather than
            with SVG <defs> avoids a gradient id per instance -- and there are
            twenty-six effects' worth of knobs on screen. */}
        <span className="knob__cap" aria-hidden="true" />
        <svg width={size} height={size} aria-hidden="true">
          {ticks.map((tick, i) => (
            <line
              className="knob__tick"
              key={i}
              x1={tick.from.x}
              y1={tick.from.y}
              x2={tick.to.x}
              y2={tick.to.y}
            />
          ))}
          {bipolar ? (
            <line
              className="knob__centre"
              x1={centreMark.from.x}
              y1={centreMark.from.y}
              x2={centreMark.to.x}
              y2={centreMark.to.y}
            />
          ) : null}
          <circle className="knob__body" cx={centre} cy={centre} r={radius - 5} />
          <path
            className="knob__track"
            d={arcPath(centre, centre, radius, START_ANGLE, START_ANGLE + ANGLE_RANGE)}
          />
          <path
            className="knob__value"
            d={arcPath(centre, centre, radius, START_ANGLE, Math.max(START_ANGLE + 0.01, angle))}
            style={{ stroke: accent }}
          />
          <line
            className="knob__pointer"
            x1={indicatorInner.x}
            y1={indicatorInner.y}
            x2={indicator.x}
            y2={indicator.y}
            style={{ stroke: accent }}
          />
        </svg>
        <span className="knob__readout">{text}</span>
      </div>
      {label ? <span className="knob__label">{label}</span> : null}
      {unit ? <span className="knob__unit">{unit}</span> : null}
    </div>
  );
}

/**
 * Memoised: the meter stream re-renders App ~22 times a second, and a rack of
 * a dozen cards holds enough dials that recomputing every arc path each frame
 * was measurable. Props are stable across a meter frame, so the memo skips it.
 */
export const Knob = memo(KnobBase);

export default Knob;
