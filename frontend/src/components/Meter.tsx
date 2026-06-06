import React, { useEffect, useState } from 'react';

export interface MeterProps {
  level: number;
  peak?: number;
  label?: string;
  stereo?: boolean;
  rightLevel?: number;
  rightPeak?: number;
}

export const Meter: React.FC<MeterProps> = ({
  level,
  peak = 0,
  label,
  stereo = false,
  rightLevel = 0,
  rightPeak = 0,
}) => {
  const [peakHold, setPeakHold] = useState(peak);
  const [rightPeakHold, setRightPeakHold] = useState(rightPeak);

  useEffect(() => {
    if (peak > peakHold) {
      setPeakHold(peak);
      const timeout = setTimeout(() => setPeakHold(0), 1000);
      return () => clearTimeout(timeout);
    }
  }, [peak, peakHold]);

  useEffect(() => {
    if (rightPeak > rightPeakHold) {
      setRightPeakHold(rightPeak);
      const timeout = setTimeout(() => setRightPeakHold(0), 1000);
      return () => clearTimeout(timeout);
    }
  }, [rightPeak, rightPeakHold]);

  const getColor = (val: number) => {
    if (val > 0.9) return '#ef4444';
    if (val > 0.7) return '#f59e0b';
    return '#10b981';
  };

  const MeterBar = ({ value, peak: peakVal }: { value: number; peak: number }) => (
    <div className="flex-1">
      <div className="h-24 bg-gray-200 dark:bg-gray-700 rounded-sm overflow-hidden relative">
        {/* RMS Level */}
        <div
          className="absolute bottom-0 left-0 right-0 transition-all duration-100"
          style={{
            height: `${value * 100}%`,
            backgroundColor: getColor(value),
          }}
        />
        {/* Peak Indicator */}
        {peakVal > 0 && (
          <div
            className="absolute left-0 right-0 h-1 bg-red-500"
            style={{
              bottom: `${Math.min(peakVal * 100, 100)}%`,
            }}
          />
        )}
      </div>
      <span className="text-xs text-gray-600 dark:text-gray-400 mt-1 block">
        {(value * 100).toFixed(0)}%
      </span>
    </div>
  );

  return (
    <div className="flex flex-col gap-2">
      {label && <span className="text-sm font-medium text-gray-700 dark:text-gray-300">{label}</span>}
      <div className="flex gap-2">
        <MeterBar value={level} peak={peakHold} />
        {stereo && <MeterBar value={rightLevel} peak={rightPeakHold} />}
      </div>
    </div>
  );
};
