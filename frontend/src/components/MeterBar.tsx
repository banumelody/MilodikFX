import React, { useRef, useEffect, useState } from 'react';

export interface MeterBarProps {
  value: number;
  min?: number;
  max?: number;
  color?: 'cyan' | 'green' | 'amber' | 'red';
  peakHold?: number;
  label?: string;
  showPeak?: boolean;
}

export const MeterBar: React.FC<MeterBarProps> = ({
  value,
  min = 0,
  max = 100,
  color = 'cyan',
  peakHold,
  label,
  showPeak = true,
}) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [peak, setPeak] = useState(peakHold || 0);

  // Normalize value to 0-100 range
  const normalizedValue = Math.max(0, Math.min(100, ((value - min) / (max - min)) * 100));
  const normalizedPeak = Math.max(0, Math.min(100, ((peak - min) / (max - min)) * 100));

  const colorMap = {
    cyan: '#06b6d4',
    green: '#10b981',
    amber: '#f59e0b',
    red: '#ef4444',
  };

  useEffect(() => {
    if (value > peak) {
      setPeak(value);
    }
  }, [value, peak]);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const w = canvas.width;
    const h = canvas.height;

    // Clear
    ctx.fillStyle = '#1f2937';
    ctx.fillRect(0, 0, w, h);

    // Background bar
    ctx.fillStyle = '#374151';
    ctx.fillRect(0, 0, w, h);

    // Value bar with gradient
    const fillWidth = (normalizedValue / 100) * w;
    const gradient = ctx.createLinearGradient(0, 0, w, 0);
    gradient.addColorStop(0, colorMap[color] + '80');
    gradient.addColorStop(1, colorMap[color]);
    ctx.fillStyle = gradient;
    ctx.fillRect(0, 0, fillWidth, h);

    // Peak line
    if (showPeak && normalizedPeak > 0) {
      const peakX = (normalizedPeak / 100) * w;
      ctx.fillStyle = colorMap[color];
      ctx.fillRect(peakX - 1, 0, 2, h);
    }

    // Border
    ctx.strokeStyle = '#4b5563';
    ctx.lineWidth = 1;
    ctx.strokeRect(0, 0, w, h);
  }, [normalizedValue, normalizedPeak, color, showPeak]);

  return (
    <div className="flex flex-col gap-1">
      {label && (
        <label className="text-xs font-medium text-gray-600 dark:text-gray-400">
          {label}
        </label>
      )}
      <canvas
        ref={canvasRef}
        width={200}
        height={16}
        className="w-full rounded border border-gray-600"
      />
    </div>
  );
};
