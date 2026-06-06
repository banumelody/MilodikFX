import { useEffect, useState } from 'react';
import PresetBar from './PresetBar';
import DevicePanel from './DevicePanel';
import AudioMeter from './AudioMeter';
import EffectCardsGrid from './EffectCardsGrid';
import StatusBar from './StatusBar';

interface Effect {
  id: string;
  type: string;
  name: string;
  enabled: boolean;
  parameters: Record<string, { value: number; min: number; max: number; unit?: string }>;
}

export default function Dashboard() {
  const [effects, setEffects] = useState<Effect[]>([]);
  const [audioMetrics, setAudioMetrics] = useState({
    inputLevelLeft: -40,
    inputLevelRight: -40,
    outputLevelLeft: -40,
    outputLevelRight: -40,
    cpuLoad: 0,
    latency: 2,
  });
  const [isConnected] = useState(true);
  const [sampleRate] = useState(48000);
  const [bufferSize] = useState(256);

  useEffect(() => {
    // Simulate real-time metrics updates
    const metricsInterval = setInterval(() => {
      setAudioMetrics(() => ({
        inputLevelLeft: -40 + Math.random() * 20,
        inputLevelRight: -40 + Math.random() * 20,
        outputLevelLeft: -40 + Math.random() * 20,
        outputLevelRight: -40 + Math.random() * 20,
        cpuLoad: Math.random() * 30,
        latency: 2 + Math.random() * 1,
      }));
    }, 200);

    return () => clearInterval(metricsInterval);
  }, []);

  const handleAddEffect = (type: string) => {
    const newEffect: Effect = {
      id: `effect-${Date.now()}`,
      type,
      name: type.charAt(0).toUpperCase() + type.slice(1),
      enabled: true,
      parameters:
        type === 'gain'
          ? {
              level: { value: 0, min: -24, max: 24, unit: ' dB' },
            }
          : type === 'overdrive'
            ? {
                drive: { value: 50, min: 0, max: 100, unit: '%' },
                level: { value: 50, min: 0, max: 100, unit: '%' },
              }
            : type === 'eq'
              ? {
                  bass: { value: 0, min: -12, max: 12, unit: ' dB' },
                  mid: { value: 0, min: -12, max: 12, unit: ' dB' },
                  treble: { value: 0, min: -12, max: 12, unit: ' dB' },
                }
              : type === 'compressor'
                ? {
                    threshold: { value: -20, min: -60, max: 0, unit: ' dB' },
                    ratio: { value: 4, min: 1, max: 20, unit: ':1' },
                  }
                : {
                    roomSize: { value: 50, min: 0, max: 100, unit: '%' },
                    mix: { value: 30, min: 0, max: 100, unit: '%' },
                  },
    };
    setEffects([...effects, newEffect]);
  };

  const handleRemoveEffect = (effectId: string) => {
    setEffects(effects.filter((e) => e.id !== effectId));
  };

  const handleParameterChange = (effectId: string, paramName: string, value: number) => {
    setEffects(
      effects.map((effect) => {
        if (effect.id === effectId) {
          return {
            ...effect,
            parameters: {
              ...effect.parameters,
              [paramName]: {
                ...effect.parameters[paramName],
                value,
              },
            },
          };
        }
        return effect;
      }),
    );
  };

  return (
    <div className="flex flex-col h-screen bg-gray-950 text-white overflow-hidden">
      {/* Top: Preset Bar */}
      <div className="p-4 border-b border-gray-800 bg-gray-900/50">
        <PresetBar />
      </div>

      {/* Main Content */}
      <div className="flex-1 overflow-y-auto">
        <div className="p-4 space-y-6 max-w-7xl mx-auto">
          {/* Upper: Device Panel */}
          <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
            <DevicePanel />
          </div>

          {/* Middle: Audio Meters (3-column layout) */}
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
            <div className="lg:col-span-3">
              <AudioMeter
                inputLevelLeft={audioMetrics.inputLevelLeft}
                inputLevelRight={audioMetrics.inputLevelRight}
                outputLevelLeft={audioMetrics.outputLevelLeft}
                outputLevelRight={audioMetrics.outputLevelRight}
                cpuLoad={audioMetrics.cpuLoad}
                latency={audioMetrics.latency}
              />
            </div>
          </div>

          {/* Center: Effect Cards Grid */}
          <EffectCardsGrid
            effects={effects}
            onAddEffect={handleAddEffect}
            onRemoveEffect={handleRemoveEffect}
            onParameterChange={handleParameterChange}
          />
        </div>
      </div>

      {/* Bottom: Status Bar */}
      <StatusBar
        isConnected={isConnected}
        cpuLoad={audioMetrics.cpuLoad}
        latency={audioMetrics.latency}
        sampleRate={sampleRate}
        bufferSize={bufferSize}
      />
    </div>
  );
}
