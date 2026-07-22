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
  label,
  unit = '',
  accent = '#4da3ff',
  disabled = false,
  format,
}: KnobProps) {
  const [dragging, setDragging] = useState(false);
  const drag = useRef<{ startY: number; startValue: number } | null>(null);

  const span = max - min || 1;

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

      const scale = (fine ? FINE_FACTOR : 1) * (span / DRAG_RANGE_PX);
      onChange(quantise(state.startValue + deltaPx * scale));
    },
    [onChange, quantise, span],
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
    const next = quantise(value + step * (event.shiftKey ? 1 : 5) * direction);
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
    const coarse = span / 10;

    switch (event.key) {
      case 'ArrowUp':
      case 'ArrowRight':
        next = value + step;
        break;
      case 'ArrowDown':
      case 'ArrowLeft':
        next = value - step;
        break;
      case 'PageUp':
        next = value + coarse;
        break;
      case 'PageDown':
        next = value - coarse;
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

  const ratio = Math.min(1, Math.max(0, (value - min) / span));
  const angle = START_ANGLE + ratio * ANGLE_RANGE;

  const centre = size / 2;
  const radius = centre - 7;
  const text = format ? format(value) : value.toFixed(decimalsForStep(step));
  const readout = unit ? `${text} ${unit}` : text;

  const indicator = polar(centre, centre, radius - 11, angle);
  const indicatorInner = polar(centre, centre, radius - 21, angle);

  return (
    <div className={`knob${disabled ? ' knob--disabled' : ''}`}>
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
        <svg width={size} height={size} aria-hidden="true">
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
