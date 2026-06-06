import React, { useRef, useEffect } from 'react';

export interface PerformanceGraphProps {
  cpuHistory: number[];
  maxValue?: number;
  showLegend?: boolean;
}

export const PerformanceGraph: React.FC<PerformanceGraphProps> = ({
  cpuHistory,
  maxValue = 100,
  showLegend = true,
}) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const w = canvas.width;
    const h = canvas.height;
    const padding = 30;
    const graphWidth = w - padding * 2;
    const graphHeight = h - padding * 2;

    // Clear
    ctx.fillStyle = '#111827';
    ctx.fillRect(0, 0, w, h);

    // Background grid
    ctx.strokeStyle = '#374151';
    ctx.lineWidth = 1;

    for (let i = 0; i <= 4; i++) {
      const y = padding + (graphHeight / 4) * i;
      ctx.beginPath();
      ctx.moveTo(padding, y);
      ctx.lineTo(w - padding, y);
      ctx.stroke();
    }

    // Axis labels
    ctx.fillStyle = '#9ca3af';
    ctx.font = '12px monospace';
    ctx.textAlign = 'right';
    for (let i = 0; i <= 4; i++) {
      const value = maxValue - (maxValue / 4) * i;
      const y = padding + (graphHeight / 4) * i;
      ctx.fillText(`${value}%`, padding - 10, y + 4);
    }

    // Draw data
    if (cpuHistory.length > 1) {
      ctx.lineWidth = 3;

      // CPU line
      ctx.strokeStyle = '#06b6d4';
      ctx.beginPath();

      for (let i = 0; i < cpuHistory.length; i++) {
        const x = padding + (graphWidth / (cpuHistory.length - 1)) * i;
        const y = h - padding - (cpuHistory[i] / maxValue) * graphHeight;

        if (i === 0) {
          ctx.moveTo(x, y);
        } else {
          ctx.lineTo(x, y);
        }
      }
      ctx.stroke();

      // Fill under curve
      ctx.fillStyle = '#06b6d480';
      ctx.lineTo(w - padding, h - padding);
      ctx.lineTo(padding, h - padding);
      ctx.fill();

      // Draw zones
      const warningThreshold = maxValue * 0.8;
      const criticalThreshold = maxValue * 0.9;

      if (cpuHistory.some((v) => v > criticalThreshold)) {
        ctx.fillStyle = '#ef444430';
        ctx.fillRect(padding, padding, graphWidth, graphHeight * (1 - criticalThreshold / maxValue));
      } else if (cpuHistory.some((v) => v > warningThreshold)) {
        ctx.fillStyle = '#f59e0b30';
        ctx.fillRect(padding, padding, graphWidth, graphHeight * (1 - warningThreshold / maxValue));
      }
    }

    // Border
    ctx.strokeStyle = '#6b7280';
    ctx.lineWidth = 1;
    ctx.strokeRect(padding, padding, graphWidth, graphHeight);

    // Axis labels
    ctx.fillStyle = '#9ca3af';
    ctx.font = '12px monospace';
    ctx.textAlign = 'center';
    ctx.fillText('Time →', w / 2, h - 5);
  }, [cpuHistory, maxValue]);

  return (
    <div className="flex flex-col gap-2">
      <div className="flex items-center justify-between">
        <h3 className="text-sm font-semibold text-gray-300">CPU Load History</h3>
        {showLegend && (
          <div className="flex gap-3 text-xs">
            <div className="flex items-center gap-1">
              <div className="w-2 h-2 bg-cyan-500 rounded" />
              <span className="text-gray-400">CPU</span>
            </div>
            <div className="flex items-center gap-1">
              <div className="w-2 h-2 bg-amber-500 rounded" />
              <span className="text-gray-400">&gt; 80%</span>
            </div>
            <div className="flex items-center gap-1">
              <div className="w-2 h-2 bg-red-500 rounded" />
              <span className="text-gray-400">&gt; 90%</span>
            </div>
          </div>
        )}
      </div>
      <canvas
        ref={canvasRef}
        width={400}
        height={200}
        className="w-full rounded border border-gray-700"
      />
    </div>
  );
};
