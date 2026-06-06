import { useState, useEffect } from 'react';
import { useDevice } from '../hooks';
import { Card, Select } from './index';

interface DevicePanelProps {
  onDeviceChanged?: (deviceId: string) => void;
}

export default function DevicePanel({ onDeviceChanged }: DevicePanelProps) {
  const { devices, loading } = useDevice();
  const [selectedDeviceId, setSelectedDeviceId] = useState<string>('');

  useEffect(() => {
    if (devices.inputs && devices.inputs.length > 0) {
      setSelectedDeviceId(devices.inputs[0]);
    }
  }, [devices]);

  const handleDeviceChange = (e: React.ChangeEvent<HTMLSelectElement>) => {
    const deviceId = e.currentTarget.value;
    setSelectedDeviceId(deviceId);
    onDeviceChanged?.(deviceId);
  };

  return (
    <Card className="bg-gradient-to-br from-gray-800 to-gray-900 rounded-lg shadow-md">
      <div className="p-4">
        <h3 className="text-sm font-semibold text-gray-300 uppercase tracking-wide mb-3">
          Audio Device
        </h3>

        {loading ? (
          <div className="text-gray-400 text-sm">Loading devices...</div>
        ) : (
          <>
            <Select
              label="Input Device"
              value={selectedDeviceId}
              options={(devices.inputs || []).map((d) => ({ value: d, label: d }))}
              onChange={handleDeviceChange}
              disabled={(devices.inputs || []).length === 0}
            />

            <div className="mt-3 pt-3 border-t border-gray-700 space-y-1">
              <div className="flex justify-between text-xs text-gray-400">
                <span>Sample Rate:</span>
                <span className="text-white font-mono">48000 Hz</span>
              </div>
              <div className="flex justify-between text-xs text-gray-400">
                <span>Buffer Size:</span>
                <span className="text-white font-mono">256 samples</span>
              </div>
              <div className="flex justify-between text-xs text-gray-400">
                <span>Latency:</span>
                <span className="text-white font-mono">5.3 ms</span>
              </div>
              <div className="flex justify-between text-xs text-gray-400">
                <span>Channels:</span>
                <span className="text-white font-mono">2in / 2out</span>
              </div>
            </div>
          </>
        )}
      </div>
    </Card>
  );
}
