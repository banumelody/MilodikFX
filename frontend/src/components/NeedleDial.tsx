import React, { useRef, useEffect } from 'react';

export interface NeedleDialProps {
  value: number; // -100 to +100 (cents)
  size?: 'sm' | 'md' | 'lg';
  color?: string;
}

export const NeedleDial: React.FC<NeedleDialProps> = ({
  value,
  size = 'md',
  color = '#06b6d4',
}) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  const sizeMap = { sm: 80, md: 120, lg: 160 };
  const canvasSize = sizeMap[size];

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const centerX = canvasSize / 2;
    const centerY = canvasSize / 2;
    const radius = canvasSize / 2 - 10;

    // Clear
    ctx.clearRect(0, 0, canvasSize, canvasSize);

    // Background circle
    ctx.fillStyle = '#1f2937';
    ctx.beginPath();
    ctx.arc(centerX, centerY, radius, 0, Math.PI * 2);
    ctx.fill();

    // Border
    ctx.strokeStyle = '#4b5563';
    ctx.lineWidth = 2;
    ctx.stroke();

    // Scale marks
    ctx.strokeStyle = '#6b7280';
    ctx.lineWidth = 1;
    for (let i = -100; i <= 100; i += 25) {
      const angle = (i / 100) * Math.PI - Math.PI / 2;
      const x1 = centerX + Math.cos(angle) * (radius - 5);
      const y1 = centerY + Math.sin(angle) * (radius - 5);
      const x2 = centerX + Math.cos(angle) * radius;
      const y2 = centerY + Math.sin(angle) * radius;
      ctx.beginPath();
      ctx.moveTo(x1, y1);
      ctx.lineTo(x2, y2);
      ctx.stroke();
    }

    // Center circle
    ctx.fillStyle = color;
    ctx.beginPath();
    ctx.arc(centerX, centerY, 5, 0, Math.PI * 2);
    ctx.fill();

    // Needle
    const angle = (value / 100) * Math.PI - Math.PI / 2;
    const needleLength = radius - 15;
    const needleX = centerX + Math.cos(angle) * needleLength;
    const needleY = centerY + Math.sin(angle) * needleLength;

    ctx.strokeStyle = color;
    ctx.lineWidth = 3;
    ctx.beginPath();
    ctx.moveTo(centerX, centerY);
    ctx.lineTo(needleX, needleY);
    ctx.stroke();
  }, [value, canvasSize]);

  return (
    <canvas
      ref={canvasRef}
      width={canvasSize}
      height={canvasSize}
      className="rounded-full"
    />
  );
};
