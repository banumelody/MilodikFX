import React from 'react';

interface StatusBarProps {
  isConnected?: boolean;
  cpuLoad?: number;
  latency?: number;
  sampleRate?: number;
  bufferSize?: number;
}

const StatusIndicator: React.FC<{ color: 'green' | 'amber' | 'red'; label: string; value: string }> =
  ({ color, label, value }) => {
    const colorMap = {
      green: 'bg-green-600',
      amber: 'bg-amber-600',
      red: 'bg-red-600',
    };

    return (
      <div className="flex items-center gap-2">
        <div className={`w-2 h-2 rounded-full ${colorMap[color]} animate-pulse`} />
        <div className="flex flex-col">
          <span className="text-xs text-gray-500">{label}</span>
          <span className="text-xs font-mono text-white">{value}</span>
        </div>
      </div>
    );
  };

export default function StatusBar({
  isConnected = true,
  cpuLoad = 0,
  latency = 0,
  sampleRate = 48000,
  bufferSize = 256,
}: StatusBarProps) {
  const getCpuColor = () => {
    if (cpuLoad > 80) return 'red';
    if (cpuLoad > 50) return 'amber';
    return 'green';
  };

  return (
    <div className="bg-gradient-to-r from-gray-900 to-gray-950 border-t border-gray-700 px-4 py-3">
      <div className="flex items-center justify-between gap-6 flex-wrap">
        {/* Connection Status */}
        <StatusIndicator
          color={isConnected ? 'green' : 'red'}
          label="Connection"
          value={isConnected ? 'Connected' : 'Disconnected'}
        />

        {/* CPU Load */}
        <StatusIndicator
          color={getCpuColor()}
          label="CPU Load"
          value={`${cpuLoad.toFixed(1)}%`}
        />

        {/* Latency */}
        <StatusIndicator
          color="green"
          label="Latency"
          value={`${latency.toFixed(1)}ms`}
        />

        {/* Sample Rate */}
        <StatusIndicator
          color="green"
          label="Sample Rate"
          value={`${(sampleRate / 1000).toFixed(0)}kHz`}
        />

        {/* Buffer Size */}
        <StatusIndicator
          color="green"
          label="Buffer"
          value={`${bufferSize} samples`}
        />

        {/* Version */}
        <div className="ml-auto">
          <span className="text-xs text-gray-600">v0.9.0-beta</span>
        </div>
      </div>
    </div>
  );
}
