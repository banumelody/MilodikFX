import { useState } from 'react';
import { Card, Button, Knob, Meter, Select, Input } from './components';
import { useTheme, useAudioEngine, useDevice } from './hooks';

function App() {
  const { theme, toggleTheme } = useTheme();
  const { isConnected, error: engineError } = useAudioEngine();
  const { devices, setInputDevice, setOutputDevice } = useDevice();
  const [gainValue, setGainValue] = useState(0);

  return (
    <div className="min-h-screen bg-white dark:bg-gray-900 transition-colors duration-200">
      {/* Header */}
      <header className="bg-gray-50 dark:bg-gray-800 border-b border-gray-200 dark:border-gray-700 p-6">
        <div className="max-w-7xl mx-auto flex justify-between items-center">
          <div>
            <h1 className="text-3xl font-bold text-gray-900 dark:text-white">MilodikFX</h1>
            <p className="text-sm text-gray-600 dark:text-gray-400 mt-1">v0.8.0 - Frontend Foundation</p>
          </div>
          <div className="flex items-center gap-4">
            <div className="flex items-center gap-2">
              <span className={`inline-block w-3 h-3 rounded-full ${isConnected ? 'bg-green-500' : 'bg-gray-400'}`} />
              <span className="text-sm text-gray-600 dark:text-gray-400">
                {isConnected ? 'Connected' : 'Disconnected'}
              </span>
            </div>
            <Button variant="ghost" size="sm" onClick={toggleTheme}>
              {theme === 'dark' ? '☀️' : '🌙'}
            </Button>
          </div>
        </div>
      </header>

      {/* Main Content */}
      <main className="max-w-7xl mx-auto p-6">
        {engineError && (
          <div className="mb-6 p-4 bg-error-50 dark:bg-red-900/20 border border-error-200 dark:border-red-700 rounded-md">
            <p className="text-error-700 dark:text-red-400 text-sm">{engineError}</p>
          </div>
        )}

        {/* Device Selection */}
        <div className="grid grid-cols-2 gap-6 mb-6">
          <Card title="Audio Input">
            <Select
              label="Input Device"
              options={devices.inputs.map((d) => ({ value: d, label: d }))}
              onChange={(e) => setInputDevice(e.currentTarget.value)}
              value={devices.selectedInput || ''}
            />
          </Card>

          <Card title="Audio Output">
            <Select
              label="Output Device"
              options={devices.outputs.map((d) => ({ value: d, label: d }))}
              onChange={(e) => setOutputDevice(e.currentTarget.value)}
              value={devices.selectedOutput || ''}
            />
          </Card>
        </div>

        {/* DSP Controls */}
        <div className="grid grid-cols-3 gap-6 mb-6">
          <Card title="Gain">
            <div className="flex flex-col items-center">
              <Knob
                value={gainValue}
                min={-24}
                max={24}
                onChange={setGainValue}
                label="Gain (dB)"
                size="lg"
              />
            </div>
          </Card>

          <Card title="Input Level">
            <Meter label="Input" level={0.45} peak={0.55} />
          </Card>

          <Card title="Output Level">
            <Meter label="Output" level={0.35} peak={0.42} />
          </Card>
        </div>

        {/* Settings */}
        <Card title="Settings">
          <div className="space-y-4">
            <Input label="Plugin Name" defaultValue="MilodikFX" />
            <div className="flex gap-2">
              <Button variant="primary">Save Settings</Button>
              <Button variant="secondary">Reset</Button>
            </div>
          </div>
        </Card>
      </main>

      {/* Footer */}
      <footer className="bg-gray-50 dark:bg-gray-800 border-t border-gray-200 dark:border-gray-700 p-6 mt-12">
        <div className="max-w-7xl mx-auto text-center text-sm text-gray-600 dark:text-gray-400">
          <p>MilodikFX © 2024 - Modern DSP Audio Plugin with React Frontend</p>
        </div>
      </footer>
    </div>
  );
}

export default App;
