import { useCallback, useEffect, useRef, useState } from 'react';

export interface RotaryKnobProps {
  value: number;
  onChange: (v: number) => void;
  min?: number;
  max?: number;
  step?: number;
  size?: number; // px
  accentColor?: string;
  label?: string;
  showValue?: boolean;
  disabled?: boolean;
}

export function RotaryKnob({
  value,
  onChange,
  min = 0,
  max = 100,
  step = 1,
  size = 72,
  accentColor = '#22c55e',
  label,
  showValue = true,
  disabled = false,
}: RotaryKnobProps) {
  const ref = useRef<HTMLDivElement | null>(null);
  const [isDragging, setIsDragging] = useState(false);

  const clamp = (v: number) => Math.min(max, Math.max(min, v));

  const startAngle = -135; // degrees
  const angleRange = 270; // degrees

  const valueToAngle = useCallback((v: number) => {
    const ratio = (v - min) / (max - min || 1);
    return startAngle + ratio * angleRange;
  }, [min, max]);

  const angleToValue = useCallback((angle: number) => {
    // normalize to [-180,180]
    let a = angle;
    if (a > 180) a -= 360;
    if (a < -180) a += 360;

    // clamp to knob range
    if (a < startAngle) a = startAngle;
    if (a > startAngle + angleRange) a = startAngle + angleRange;

    const ratio = (a - startAngle) / angleRange;
    const raw = min + ratio * (max - min || 1);
    // snap to step
    const stepped = Math.round(raw / step) * step;
    return clamp(stepped);
  }, [min, max, step]);

  const handlePointer = useCallback((clientX: number, clientY: number) => {
    const el = ref.current;
    if (!el) return;
    const rect = el.getBoundingClientRect();
    const cx = rect.left + rect.width / 2;
    const cy = rect.top + rect.height / 2;
    const dx = clientX - cx;
    const dy = clientY - cy;
    const angle = Math.atan2(dy, dx) * 180 / Math.PI; // -180..180, 0 = right
    const newVal = angleToValue(angle);
    if (newVal !== value) onChange(newVal);
  }, [angleToValue, onChange, value]);

  useEffect(() => {
    if (!isDragging) return;
    const onMove = (e: PointerEvent) => {
      handlePointer(e.clientX, e.clientY);
    };
    const onUp = () => setIsDragging(false);
    window.addEventListener('pointermove', onMove);
    window.addEventListener('pointerup', onUp);
    return () => {
      window.removeEventListener('pointermove', onMove);
      window.removeEventListener('pointerup', onUp);
    };
  }, [isDragging, handlePointer]);

  const onPointerDown = (e: React.PointerEvent) => {
    if (disabled) return;
    (e.target as Element).setPointerCapture?.(e.pointerId);
    setIsDragging(true);
    handlePointer(e.clientX, e.clientY);
  };

  const onKeyDown = (e: React.KeyboardEvent) => {
    if (disabled) return;
    let delta = 0;
    if (e.key === 'ArrowUp' || e.key === 'ArrowRight') delta = step;
    if (e.key === 'ArrowDown' || e.key === 'ArrowLeft') delta = -step;
    if (e.key === 'PageUp') delta = (max - min) / 10;
    if (e.key === 'PageDown') delta = -(max - min) / 10;
    if (delta !== 0) {
      const newVal = clamp(Math.round((value + delta) / step) * step);
      onChange(newVal);
      e.preventDefault();
    }
  };

  // visual values
  const percent = (value - min) / (max - min || 1);
  const angle = valueToAngle(value);
  const accent = accentColor;

  const wrapperStyle: React.CSSProperties = {
    width: size,
    height: size + (label ? 22 : 0),
    display: 'flex',
    flexDirection: 'column',
    alignItems: 'center',
    gap: 6,
    userSelect: 'none'
  };

  const knobStyle: React.CSSProperties = {
    width: size,
    height: size,
    borderRadius: '50%',
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    position: 'relative',
    background: 'radial-gradient(circle at 35% 25%, #20242b 0%, #0b0f14 45%, #050609 100%)',
    border: '1px solid rgba(255,255,255,0.03)',
    boxShadow: 'inset 0 6px 14px rgba(0,0,0,0.75), 0 10px 30px rgba(0,0,0,0.75)',
    overflow: 'hidden'
  };

  const ringStyle: React.CSSProperties = {
    position: 'absolute',
    left: 4,
    right: 4,
    top: 4,
    bottom: 4,
    borderRadius: '50%',
    background: `conic-gradient(${accent} 0deg ${percent * angleRange}deg, rgba(255,255,255,0.04) ${percent * angleRange}deg 360deg)`,
    transform: `rotate(${startAngle}deg)`,
    zIndex: 1,
    boxShadow: 'inset 0 1px 0 rgba(255,255,255,0.02), 0 1px 2px rgba(0,0,0,0.6)'
  };

  const ringInnerGlowStyle: React.CSSProperties = {
    position: 'absolute',
    left: 8,
    right: 8,
    top: 8,
    bottom: 8,
    borderRadius: '50%',
    zIndex: 2,
    pointerEvents: 'none',
    background: 'radial-gradient(circle at 30% 20%, rgba(255,255,255,0.03), transparent 18%)'
  };

  const innerStyle: React.CSSProperties = {
    width: Math.floor(size * 0.6),
    height: Math.floor(size * 0.6),
    borderRadius: '50%',
    background: 'linear-gradient(180deg, #131823 0%, #081219 100%)',
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    zIndex: 3,
    boxShadow: 'inset 0 2px 8px rgba(0,0,0,0.7)'
  };

  const indicatorStyle: React.CSSProperties = {
    position: 'absolute',
    width: Math.max(4, Math.floor(size * 0.09)),
    height: Math.max(10, Math.floor(size * 0.28)),
    background: 'linear-gradient(180deg, rgba(255,255,255,0.95) 0%, rgba(255,255,255,0.65) 30%, rgba(0,0,0,0.6) 100%)',
    borderRadius: 2,
    top: 6,
    left: '50%',
    transformOrigin: `50% ${size / 2 - 6}px`,
    transform: `translateX(-50%) rotate(${angle}deg)`,
    zIndex: 6,
    boxShadow: '0 6px 12px rgba(0,0,0,0.6)'
  };

  const centerDotStyle: React.CSSProperties = {
    width: Math.floor(size * 0.34),
    height: Math.floor(size * 0.34),
    borderRadius: '50%',
    background: 'radial-gradient(circle at 35% 25%, rgba(255,255,255,0.08), rgba(0,0,0,0.5))',
    zIndex: 5,
    boxShadow: 'inset 0 2px 8px rgba(0,0,0,0.7), 0 4px 10px rgba(0,0,0,0.35)'
  };

  const topHighlightStyle: React.CSSProperties = {
    position: 'absolute',
    left: 10,
    right: 10,
    top: 8,
    height: Math.max(8, Math.floor(size * 0.12)),
    borderRadius: 999,
    zIndex: 4,
    pointerEvents: 'none',
    background: 'linear-gradient(180deg, rgba(255,255,255,0.06), rgba(255,255,255,0.01))',
    mixBlendMode: 'screen'
  };

  const labelStyle: React.CSSProperties = {
    color: '#9aa4b2',
    fontSize: 12,
    textAlign: 'center'
  };

  const valueStyle: React.CSSProperties = {
    color: '#e6eef6',
    fontSize: 12,
    marginTop: 2,
  };

  return (
    <div style={wrapperStyle}>
      <div
        ref={ref}
        role="slider"
        aria-valuemin={min}
        aria-valuemax={max}
        aria-valuenow={Math.round(value)}
        tabIndex={disabled ? -1 : 0}
        onKeyDown={onKeyDown}
        onPointerDown={onPointerDown}
        style={knobStyle}
      >
        {/* colored arc ring */}
        <div style={ringStyle} />

        <div style={ringInnerGlowStyle} />
        <div style={topHighlightStyle} />

        {/* pointer */}
        <div style={indicatorStyle} />

        {/* inner body */}
        <div style={innerStyle} />

        {/* center cap */}
        <div style={centerDotStyle} />

      </div>

      {label ? <div style={labelStyle}>{label}</div> : null}
      {showValue ? <div style={valueStyle}>{Math.round(value)}</div> : null}
    </div>
  );
}

export default RotaryKnob;
