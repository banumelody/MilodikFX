import React, { useRef, useEffect, useState } from 'react';

export interface KnobProps {
  value: number;
  min: number;
  max: number;
  onChange: (value: number) => void;
  label?: string;
  size?: 'sm' | 'md' | 'lg';
  disabled?: boolean;
  step?: number;
}

export const Knob: React.FC<KnobProps> = ({
  value,
  min,
  max,
  onChange,
  label,
  size = 'md',
  disabled = false,
  step = 0.1,
}) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [isDragging, setIsDragging] = useState(false);
  const [lastY, setLastY] = useState(0);

  const sizeMap = { sm: 60, md: 80, lg: 100 };
  const radius = sizeMap[size] / 2;

  const normalize = (v: number) => (v - min) / (max - min);

  const drawKnob = () => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const w = canvas.width;
    const h = canvas.height;
    ctx.clearRect(0, 0, w, h);

    const cx = w / 2;
    const cy = h / 2;
    const innerRadius = radius * 0.7;
    const outerRadius = radius * 0.9;

    // Background circle
    ctx.fillStyle = disabled ? '#d1d5db' : '#e5e7eb';
    ctx.beginPath();
    ctx.arc(cx, cy, outerRadius, 0, Math.PI * 2);
    ctx.fill();

    // Value arc
    const normalized = normalize(value);
    const startAngle = -Math.PI * 0.75;
    const endAngle = startAngle + Math.PI * 1.5 * normalized;

    ctx.strokeStyle = disabled ? '#9ca3af' : '#3b82f6';
    ctx.lineWidth = 6;
    ctx.lineCap = 'round';
    ctx.beginPath();
    ctx.arc(cx, cy, radius * 0.8, startAngle, endAngle);
    ctx.stroke();

    // Center circle
    ctx.fillStyle = disabled ? '#9ca3af' : '#1f2937';
    ctx.beginPath();
    ctx.arc(cx, cy, innerRadius, 0, Math.PI * 2);
    ctx.fill();

    // Indicator
    const indicatorAngle = startAngle + Math.PI * 1.5 * normalized;
    const indicatorRadius = radius * 0.6;
    const ix = cx + Math.cos(indicatorAngle) * indicatorRadius;
    const iy = cy + Math.sin(indicatorAngle) * indicatorRadius;

    ctx.fillStyle = '#ffffff';
    ctx.beginPath();
    ctx.arc(ix, iy, 4, 0, Math.PI * 2);
    ctx.fill();
  };

  useEffect(() => {
    drawKnob();
  }, [value, disabled]);

  const handleMouseDown = (e: React.MouseEvent) => {
    if (disabled) return;
    setIsDragging(true);
    setLastY(e.clientY);
  };

  const handleMouseMove = (e: MouseEvent) => {
    if (!isDragging || disabled) return;

    const dy = lastY - e.clientY;
    const sensitivity = 0.01;
    const delta = dy * sensitivity * (max - min);
    const newValue = Math.max(min, Math.min(max, value + delta));

    onChange(Math.round((newValue / step) * 100) / 100);
    setLastY(e.clientY);
  };

  const handleMouseUp = () => {
    setIsDragging(false);
  };

  useEffect(() => {
    if (isDragging) {
      window.addEventListener('mousemove', handleMouseMove);
      window.addEventListener('mouseup', handleMouseUp);
      return () => {
        window.removeEventListener('mousemove', handleMouseMove);
        window.removeEventListener('mouseup', handleMouseUp);
      };
    }
  }, [isDragging, value, disabled]);

  return (
    <div className="flex flex-col items-center gap-2">
      <canvas
        ref={canvasRef}
        width={sizeMap[size]}
        height={sizeMap[size]}
        onMouseDown={handleMouseDown}
        className={`cursor-${disabled ? 'not-allowed' : 'grab'} active:cursor-grabbing rounded-full`}
      />
      {label && <label className="text-sm font-medium text-gray-700 dark:text-gray-300">{label}</label>}
      <span className="text-xs text-gray-600 dark:text-gray-400">{value.toFixed(2)}</span>
    </div>
  );
};
