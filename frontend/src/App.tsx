import { useCallback, useEffect, useMemo, useRef, useState } from 'react';

import { DeviceSettings } from './components/DeviceSettings';
import { EffectRack, EFFECT_ACCENTS } from './components/EffectRack';
import { Knob } from './components/Knob';
import { LevelMeter, ReductionMeter } from './components/LevelMeter';
import { PresetControls } from './components/PresetControls';
import {
  deletePreset,
  getDevices,
  getEffects,
  getPresets,
  loadPreset,
  optimiseDevice,
  savePreset,
  setDevice,
  setEffectEnabled,
  setParameter,
  subscribeLevels,
} from './services/api';
import type {
  DeviceRequest,
  DevicesResponse,
  EffectDescriptor,
  Levels,
} from './services/api';

type Connection = 'connecting' | 'online' | 'offline';

/** Coalescing window for parameter writes, in ms. */
const WRITE_INTERVAL_MS = 40;

const IDLE_LEVELS: Levels = {
  inputLevel: -100,
  outputLevel: -100,
  gateGain: 1,
  compressorReductionDb: 0,
  limiterReductionDb: 0,
  cpuPercent: 0,
  sampleRate: 0,
  bufferSize: 0,
  audioRunning: false,
  floorDb: -100,
};

function describeError(error: unknown) {
  if (error instanceof Error) return error.message;
  return String(error);
}

export function App() {
  const [effects, setEffects] = useState<EffectDescriptor[]>([]);
  const [levels, setLevels] = useState<Levels>(IDLE_LEVELS);
  const [devices, setDevices] = useState<DevicesResponse | null>(null);
  const [presets, setPresets] = useState<string[]>([]);
  const [selectedPreset, setSelectedPreset] = useState('');
  const [connection, setConnection] = useState<Connection>('connecting');
  const [message, setMessage] = useState<string | null>(null);
  const [deviceError, setDeviceError] = useState<string | null>(null);
  const [deviceBusy, setDeviceBusy] = useState(false);

  // Writes are coalesced per parameter: dragging a knob fires a pointermove per
  // frame, and the engine runs one thread per connection.
  const pendingWrites = useRef(new Map<string, { effect: string; parameter: string; value: number }>());
  const flushTimer = useRef<number | null>(null);

  const refreshEffects = useCallback(async () => {
    const response = await getEffects();
    setEffects(response.effects);
  }, []);

  const refreshDevices = useCallback(async () => {
    try {
      setDevices(await getDevices());
      setDeviceError(null);
    } catch (error) {
      setDeviceError(describeError(error));
    }
  }, []);

  const refreshPresets = useCallback(async () => {
    const response = await getPresets();
    setPresets(response.presets);
    setSelectedPreset(response.selected);
  }, []);

  useEffect(() => {
    let cancelled = false;

    const bootstrap = async () => {
      try {
        await Promise.all([refreshEffects(), refreshDevices(), refreshPresets()]);
        if (!cancelled) setConnection('online');
      } catch (error) {
        if (!cancelled) {
          setConnection('offline');
          setMessage(describeError(error));
        }
      }
    };

    void bootstrap();
    return () => {
      cancelled = true;
    };
  }, [refreshEffects, refreshDevices, refreshPresets]);

  useEffect(() => {
    const unsubscribe = subscribeLevels(
      (next) => {
        setLevels(next);
        setConnection('online');
      },
      () => setConnection('offline'),
    );

    return unsubscribe;
  }, []);

  const flushWrites = useCallback(async () => {
    flushTimer.current = null;

    const batch = Array.from(pendingWrites.current.values());
    pendingWrites.current.clear();

    for (const write of batch) {
      try {
        await setParameter(write.effect, write.parameter, write.value);
      } catch (error) {
        setConnection('offline');
        setMessage(describeError(error));
        return;
      }
    }

    if (pendingWrites.current.size > 0 && flushTimer.current === null)
      flushTimer.current = window.setTimeout(() => void flushWrites(), WRITE_INTERVAL_MS);
  }, []);

  const handleParameterChange = useCallback(
    (effectId: string, parameterId: string, value: number) => {
      // Update locally first so the knob tracks the pointer even if the engine
      // is momentarily slow; the value is authoritative on the next refresh.
      setEffects((current) =>
        current.map((effect) =>
          effect.id === effectId
            ? {
                ...effect,
                parameters: effect.parameters.map((parameter) =>
                  parameter.id === parameterId ? { ...parameter, value } : parameter,
                ),
              }
            : effect,
        ),
      );

      pendingWrites.current.set(`${effectId}.${parameterId}`, {
        effect: effectId,
        parameter: parameterId,
        value,
      });

      if (flushTimer.current === null)
        flushTimer.current = window.setTimeout(() => void flushWrites(), WRITE_INTERVAL_MS);
    },
    [flushWrites],
  );

  const handleEnabledChange = useCallback(async (effectId: string, enabled: boolean) => {
    setEffects((current) =>
      current.map((effect) => (effect.id === effectId ? { ...effect, enabled } : effect)),
    );

    try {
      await setEffectEnabled(effectId, enabled);
    } catch (error) {
      setConnection('offline');
      setMessage(describeError(error));
    }
  }, []);

  const handleDeviceApply = useCallback(
    async (request: DeviceRequest) => {
      setDeviceBusy(true);
      setDeviceError(null);

      try {
        await setDevice(request);
        await refreshDevices();
      } catch (error) {
        setDeviceError(describeError(error));
        await refreshDevices();
      } finally {
        setDeviceBusy(false);
      }
    },
    [refreshDevices],
  );

  const handleOptimise = useCallback(async () => {
    setDeviceBusy(true);
    setDeviceError(null);

    try {
      const result = await optimiseDevice();
      await refreshDevices();

      const ms = result.current.roundTripLatencyMs;
      setMessage(`Latensi sekarang ${ms.toFixed(1)} ms (${result.current.bufferSize} sampel)`);
      window.setTimeout(() => setMessage(null), 4000);
    } catch (error) {
      setDeviceError(describeError(error));
      await refreshDevices();
    } finally {
      setDeviceBusy(false);
    }
  }, [refreshDevices]);

  const withMessage = useCallback(
    async (action: () => Promise<void>, success: string) => {
      try {
        await action();
        setMessage(success);
        window.setTimeout(() => setMessage(null), 2500);
      } catch (error) {
        setMessage(describeError(error));
      }
    },
    [],
  );

  const handlePresetLoad = useCallback(
    (name: string) =>
      void withMessage(async () => {
        await loadPreset(name);
        await refreshEffects();
        setSelectedPreset(name);
      }, `Preset "${name}" dimuat`),
    [refreshEffects, withMessage],
  );

  const handlePresetSave = useCallback(
    (name: string) =>
      void withMessage(async () => {
        await savePreset(name);
        await refreshPresets();
      }, `Preset "${name}" disimpan`),
    [refreshPresets, withMessage],
  );

  const handlePresetDelete = useCallback(
    (name: string) =>
      void withMessage(async () => {
        await deletePreset(name);
        await refreshPresets();
      }, `Preset "${name}" dihapus`),
    [refreshPresets, withMessage],
  );

  const master = useMemo(() => effects.find((effect) => effect.id === 'master'), [effects]);
  const masterVolume = master?.parameters.find((parameter) => parameter.id === 'volumeDb');

  const offline = connection === 'offline';

  return (
    <div className="app">
      <header className="topbar">
        <div className="topbar__brand">
          <span className="topbar__logo" aria-hidden="true" />
          <div>
            <h1>MilodikFX</h1>
            <p>{levels.audioRunning ? 'Audio berjalan' : 'Audio berhenti'}</p>
          </div>
        </div>

        <div className="topbar__meters">
          <LevelMeter label="Input" db={levels.inputLevel} floorDb={levels.floorDb} />
          <LevelMeter label="Output" db={levels.outputLevel} floorDb={levels.floorDb} />
          <ReductionMeter label="Comp" db={levels.compressorReductionDb} />
          <ReductionMeter label="Limiter" db={levels.limiterReductionDb} />
        </div>

        <div className="topbar__master">
          {masterVolume ? (
            <Knob
              value={masterVolume.value}
              min={masterVolume.min}
              max={masterVolume.max}
              step={masterVolume.step}
              defaultValue={masterVolume.default}
              label="Master"
              unit="dB"
              size={88}
              accent={EFFECT_ACCENTS.master}
              disabled={offline}
              format={(value) => (value <= masterVolume.min ? 'MUTE' : value.toFixed(1))}
              onChange={(value) => handleParameterChange('master', 'volumeDb', value)}
            />
          ) : null}
        </div>

        <div className={`status status--${connection}`}>
          <span className="status__dot" aria-hidden="true" />
          <span>
            {connection === 'online'
              ? 'Terhubung'
              : connection === 'connecting'
                ? 'Menghubungkan...'
                : 'Terputus'}
          </span>
        </div>
      </header>

      {offline ? (
        <div className="banner banner--error" role="alert">
          Tidak dapat menghubungi engine audio. Pastikan MilodikFX masih berjalan.
        </div>
      ) : null}

      {message ? (
        <div className="banner" role="status">
          {message}
        </div>
      ) : null}

      <main className="layout">
        <div className="layout__chain">
          {effects.length === 0 && !offline ? (
            <p className="layout__empty">Memuat rantai efek...</p>
          ) : null}

          {effects.map((effect) => (
            <EffectRack
              key={effect.id}
              effect={effect}
              disabled={offline}
              onParameterChange={handleParameterChange}
              onEnabledChange={(id, enabled) => void handleEnabledChange(id, enabled)}
            />
          ))}
        </div>

        <aside className="layout__side">
          <DeviceSettings
            devices={devices}
            busy={deviceBusy}
            error={deviceError}
            onApply={(request) => void handleDeviceApply(request)}
            onRefresh={() => void refreshDevices()}
            onOptimise={() => void handleOptimise()}
          />

          <PresetControls
            presets={presets}
            selected={selectedPreset}
            busy={offline}
            onLoad={handlePresetLoad}
            onSave={handlePresetSave}
            onDelete={handlePresetDelete}
          />

          <section className="panel" aria-label="Performa">
            <header className="panel__head">
              <h2 className="panel__title">Performa</h2>
            </header>
            <dl className="stats">
              <div>
                <dt>Beban DSP</dt>
                <dd className={levels.cpuPercent > 70 ? 'stats--warn' : undefined}>
                  {levels.cpuPercent.toFixed(1)} %
                </dd>
              </div>
              <div>
                <dt>Sample rate</dt>
                <dd>{levels.sampleRate ? `${(levels.sampleRate / 1000).toFixed(1)} kHz` : '--'}</dd>
              </div>
              <div>
                <dt>Buffer</dt>
                <dd>
                  {levels.bufferSize
                    ? `${levels.bufferSize} (${((levels.bufferSize / (levels.sampleRate || 1)) * 1000).toFixed(1)} ms)`
                    : '--'}
                </dd>
              </div>
              <div>
                <dt>Gate</dt>
                <dd>{levels.gateGain > 0.99 ? 'Terbuka' : levels.gateGain < 0.01 ? 'Tertutup' : 'Menutup'}</dd>
              </div>
            </dl>
          </section>
        </aside>
      </main>
    </div>
  );
}

export default App;
