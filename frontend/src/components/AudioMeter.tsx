import { useState, useEffect, useRef } from 'react';
import { Card } from './index';

interface AudioMeterProps {
  inputLevelLeft?: number;
  inputLevelRight?: number;
  outputLevelLeft?: number;
  outputLevelRight?: number;
  cpuLoad?: number;
  latency?: number;
  peakHoldTime?: number;
}

const MeterBar: React.FC<{ level: number; label: string; peak?: number }> = ({
  level,
  label,
  peak,
}) => (
  <div className="mb-3">
    <div className="flex justify-between items-center mb-1">
      <span className="text-xs font-medium text-gray-300">{label}</span>
      <span className="text-xs text-gray-500">{level.toFixed(1)} dB</span>
    </div>
    <div className="relative h-6 bg-gray-800 rounded overflow-hidden border border-gray-700">
      <div
        className="h-full bg-gradient-to-r from-green-500 via-yellow-500 to-red-500 transition-all duration-75"
        style={{ width: `${Math.min(100, Math.max(0, level + 60) / 60 * 100)}%` }}
      />
      {peak !== undefined && peak > 0 && (
        <div
          className="absolute top-0 bottom-0 w-0.5 bg-red-600"
          style={{ left: `${Math.min(100, Math.max(0, peak + 60) / 60 * 100)}%` }}
        />
      )}
    </div>
  </div>
);

export default function AudioMeter({
  inputLevelLeft = -40,
  inputLevelRight = -40,
  outputLevelLeft = -40,
  outputLevelRight = -40,
  cpuLoad = 0,
  latency = 0,
  peakHoldTime = 3000,
}: AudioMeterProps) {
  const [peaks, setPeaks] = useState({ inputL: -60, inputR: -60, outputL: -60, outputR: -60 });
  const peakTimeoutsRef = useRef<Record<string, NodeJS.Timeout>>({});

  useEffect(() => {
    const updatePeak = (key: string, value: number) => {
      if (value > peaks[key as keyof typeof peaks]) {
        setPeaks((p) => ({ ...p, [key]: value }));

        if (peakTimeoutsRef.current[key]) {
          clearTimeout(peakTimeoutsRef.current[key]);
        }

        peakTimeoutsRef.current[key] = setTimeout(() => {
          setPeaks((p) => ({ ...p, [key]: -60 }));
        }, peakHoldTime);
      }
    };

    updatePeak('inputL', inputLevelLeft);
    updatePeak('inputR', inputLevelRight);
    updatePeak('outputL', outputLevelLeft);
    updatePeak('outputR', outputLevelRight);
  }, [inputLevelLeft, inputLevelRight, outputLevelLeft, outputLevelRight, peakHoldTime]);

  const cpuColor = cpuLoad > 80 ? 'text-red-400' : cpuLoad > 50 ? 'text-yellow-400' : 'text-green-400';

  return (
    <Card className="bg-gradient-to-br from-gray-900 to-gray-950 rounded-lg shadow-lg p-4">
      <h3 className="text-sm font-semibold text-gray-300 uppercase tracking-wide mb-4">
        📊 Audio Metering
      </h3>

      <div className="grid grid-cols-2 gap-6">
        {/* Input Levels */}
        <div>
          <p className="text-xs font-semibold text-gray-400 mb-3">Input</p>
          <MeterBar label="L" level={inputLevelLeft} peak={peaks.inputL} />
          <MeterBar label="R" level={inputLevelRight} peak={peaks.inputR} />
        </div>

        {/* Output Levels */}
        <div>
          <p className="text-xs font-semibold text-gray-400 mb-3">Output</p>
          <MeterBar label="L" level={outputLevelLeft} peak={peaks.outputL} />
          <MeterBar label="R" level={outputLevelRight} peak={peaks.outputR} />
        </div>
      </div>

      {/* Metrics */}
      <div className="grid grid-cols-2 gap-4 mt-6 pt-4 border-t border-gray-700">
        <div>
          <p className="text-xs text-gray-500 mb-1">CPU Load</p>
          <p className={`text-lg font-bold ${cpuColor}`}>{cpuLoad.toFixed(1)}%</p>
        </div>
        <div>
          <p className="text-xs text-gray-500 mb-1">Latency</p>
          <p className="text-lg font-bold text-blue-400">{latency.toFixed(1)} ms</p>
        </div>
      </div>
    </Card>
  );
}
