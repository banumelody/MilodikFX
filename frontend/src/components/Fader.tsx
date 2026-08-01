import { useCallback, useEffect, useRef, useState } from 'react';

/** Pixels of vertical travel that span the whole range. Matches Knob. */
const DRAG_RANGE_PX = 220;

/** Shift slows the drag by this much, as it does on a knob. */
const FINE_FACTOR = 0.2;

export interface FaderProps {
  value: number;
  min?: number;
  max?: number;
  step?: number;
  defaultValue?: number;
  label?: string;
  accent?: string;
  disabled?: boolean;
  format?: (value: number) => string;
  onChange: (value: number) => void;
  onCommit?: (value: number) => void;
}

/**
 * A vertical fader.
 *
 * Used in exactly one place: the Mixer's two bus levels. That is the only
 * console function in the app, and the only place where the task is comparing
 * two values *against each other* -- two heights are far easier to compare than
 * two rotations. Everywhere else the hardware is a knob, so the app is too.
 *
 * The gesture is deliberately identical to `Knob`: a relative vertical drag from
 * the press point, shift to slow it, wheel, double-click to default, and the
 * same keyboard set. A fader that behaved differently from the knob beside it
 * would be a second thing to remember for no gain.
 */
export function Fader({
  value,
  min = 0,
  max = 1,
  step = 0.01,
  defaultValue,
  label,
  accent = '#4da3ff',
  disabled = false,
  format,
  onChange,
  onCommit,
}: FaderProps) {
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
      // Up is more, which is the only direction a fader can mean.
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
  const text = format ? format(value) : value.toFixed(2);

  return (
    <div className={`fader${disabled ? ' fader--disabled' : ''}`}>
      <div
        className={`fader__track${dragging ? ' fader__track--active' : ''}`}
        role="slider"
        aria-label={label}
        aria-valuemin={min}
        aria-valuemax={max}
        aria-valuenow={Number(value.toFixed(3))}
        aria-valuetext={text}
        aria-disabled={disabled}
        tabIndex={disabled ? -1 : 0}
        onPointerDown={onPointerDown}
        onWheel={onWheel}
        onDoubleClick={onDoubleClick}
        onKeyDown={onKeyDown}
        style={{ touchAction: 'none' }}
      >
        {/* Scale marks, for the same reason the knob has them: the drag is
            relative, so the drawing is the only absolute reference. */}
        <span className="fader__scale" aria-hidden="true">
          {Array.from({ length: 5 }, (_, i) => (
            <i key={i} />
          ))}
        </span>

        <span className="fader__fill" style={{ height: `${ratio * 100}%`, background: accent }} />
        <span className="fader__cap" style={{ bottom: `calc(${ratio * 100}% - 7px)` }} />
      </div>

      <span className="fader__readout">{text}</span>
      {label ? <span className="fader__label">{label}</span> : null}
    </div>
  );
}

export default Fader;
