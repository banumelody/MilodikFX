import { useCallback, useEffect, useMemo, useRef, useState } from 'react';

import { ChainStrip } from './components/ChainStrip';
import { DeviceSettings } from './components/DeviceSettings';
import { EffectRack, EFFECT_ACCENTS } from './components/EffectRack';
import { Knob } from './components/Knob';
import { LevelMeter, ReductionMeter } from './components/LevelMeter';
import { MidiMapping } from './components/MidiMapping';
import { NamPanel } from './components/NamPanel';
import { PresetControls } from './components/PresetControls';
import { SceneGrid } from './components/SceneGrid';
import { Sparkline } from './components/Sparkline';
import { TempoPanel } from './components/TempoPanel';
import { TunerDisplay } from './components/TunerDisplay';
import {
  deletePreset,
  exportPreset,
  getDevices,
  getEffects,
  getHistory,
  getPresets,
  importPreset,
  loadPreset,
  optimiseDevice,
  redoChange,
  revealIrFolder,
  savePreset,
  ApiError,
  setDevice,
  setEffectEnabled,
  setParameter,
  setPresetMetadata,
  subscribeLevels,
  undoChange,
} from './services/api';
import type {
  DeviceRequest,
  DevicesResponse,
  EffectDescriptor,
  HistoryState,
  Levels,
  PresetMetadata,
} from './services/api';

type Connection = 'connecting' | 'online' | 'offline';

/** Coalescing window for parameter writes, in ms. */
const WRITE_INTERVAL_MS = 40;

/** How many CPU samples the history plot keeps (~60 s at 100 ms polling). */
const CPU_HISTORY_LENGTH = 600;

const IDLE_LEVELS: Levels = {
  inputLevel: -100,
  chainInputLevel: -100,
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
  const [cpuHistory, setCpuHistory] = useState<number[]>([]);
  const [devices, setDevices] = useState<DevicesResponse | null>(null);
  const [presets, setPresets] = useState<string[]>([]);
  const [presetDetails, setPresetDetails] = useState<PresetMetadata[]>([]);
  const [selectedPreset, setSelectedPreset] = useState('');
  const [connection, setConnection] = useState<Connection>('connecting');
  const [message, setMessage] = useState<string | null>(null);
  const [deviceError, setDeviceError] = useState<string | null>(null);
  const [deviceBusy, setDeviceBusy] = useState(false);
  const [history, setHistory] = useState<HistoryState>({
    canUndo: false,
    canRedo: false,
    undoDepth: 0,
    redoDepth: 0,
  });

  // Writes are coalesced per parameter: dragging a knob fires a pointermove per
  // frame, and the engine runs one thread per connection.
  const pendingWrites = useRef(
    new Map<string, { effect: string; parameter: string; value: number | string }>(),
  );
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
    setPresetDetails(response.details ?? []);
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
        setCpuHistory((history) => {
          const appended = [...history, next.cpuPercent];
          return appended.length > CPU_HISTORY_LENGTH
            ? appended.slice(appended.length - CPU_HISTORY_LENGTH)
            : appended;
        });
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
    (effectId: string, parameterId: string, value: number | string) => {
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

  const withMessage = useCallback(async (action: () => Promise<void>, success: string) => {
    try {
      await action();
      setMessage(success);
      window.setTimeout(() => setMessage(null), 2500);
    } catch (error) {
      setMessage(describeError(error));
    }
  }, []);

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

  const handleMetadataChange = useCallback(
    (name: string, changes: Parameters<typeof setPresetMetadata>[1]) =>
      void withMessage(async () => {
        await setPresetMetadata(name, changes);
        await refreshPresets();
      }, 'Info preset diperbarui'),
    [refreshPresets, withMessage],
  );

  const handlePresetExport = useCallback(
    (name: string) =>
      void withMessage(async () => {
        const exported = await exportPreset(name);

        // A Blob download rather than writing a file from the engine: the
        // browser already knows where this machine puts downloads.
        const url = URL.createObjectURL(
          new Blob([exported.data], { type: 'application/json' }),
        );

        const link = document.createElement('a');
        link.href = url;
        link.download = exported.filename;
        document.body.appendChild(link);
        link.click();
        link.remove();

        // Revoked on the next tick; doing it immediately can cancel the
        // download before it starts.
        window.setTimeout(() => URL.revokeObjectURL(url), 1000);
      }, 'Preset diekspor'),
    [withMessage],
  );

  const handlePresetImport = useCallback(
    (name: string, data: string) =>
      void withMessage(async () => {
        await importPreset(name, data);
        await refreshPresets();
      }, `Preset "${name}" diimpor`),
    [refreshPresets, withMessage],
  );

  const handleRevealIr = useCallback(
    () =>
      void withMessage(async () => {
        await revealIrFolder('cabinet');
        // The folder is open; a refresh picks up whatever was dropped into it.
        window.setTimeout(() => void refreshEffects(), 1500);
      }, 'Folder impulse response dibuka'),
    [refreshEffects, withMessage],
  );

  const master = useMemo(() => effects.find((effect) => effect.id === 'master'), [effects]);
  const masterVolume = master?.parameters.find((parameter) => parameter.id === 'volumeDb');
  const masterMuted = master?.parameters.find((parameter) => parameter.id === 'muted');

  const global = useMemo(() => effects.find((effect) => effect.id === 'global'), [effects]);
  const bypass = global?.parameters.find((parameter) => parameter.id === 'bypass');
  const bpm = global?.parameters.find((parameter) => parameter.id === 'bpm');

  const metronome = useMemo(
    () => effects.find((effect) => effect.id === 'metronome'),
    [effects],
  );

  const offline = connection === 'offline';
  const isMuted = Number(masterMuted?.value ?? 0) >= 0.5;
  const isBypassed = Number(bypass?.value ?? 0) >= 0.5;

  const toggleMute = useCallback(() => {
    if (offline || !masterMuted) return;
    handleParameterChange('master', 'muted', isMuted ? 0 : 1);
  }, [offline, masterMuted, isMuted, handleParameterChange]);

  const toggleBypass = useCallback(() => {
    if (offline || !bypass) return;
    handleParameterChange('global', 'bypass', isBypassed ? 0 : 1);
  }, [offline, bypass, isBypassed, handleParameterChange]);

  const refreshHistory = useCallback(async () => {
    try {
      setHistory(await getHistory());
    } catch {
      /* the connection banner already says the engine is unreachable */
    }
  }, []);

  // The engine commits a step once the chain has been still for a moment, so
  // the buttons cannot know they have become available without asking.
  useEffect(() => {
    const timer = window.setInterval(() => void refreshHistory(), 1500);
    void refreshHistory();
    return () => window.clearInterval(timer);
  }, [refreshHistory]);

  const applyHistory = useCallback(
    (action: () => Promise<HistoryState>) =>
      void (async () => {
        try {
          const next = await action();
          setHistory(next);

          // The response carries the effects, so there is no second round trip
          // and no window where the rack still shows the pre-undo values.
          if (next.effects) setEffects(next.effects);
        } catch (error) {
          // 409 just means there was nothing to undo, which a keyboard
          // shortcut does all the time. Not worth a banner.
          if (!(error instanceof ApiError) || error.status !== 409)
            setMessage(describeError(error));
        }
      })(),
    [],
  );

  // Panic controls have to be reachable without hunting for a card. Escape mutes
  // and B compares against the dry signal; both are ignored while typing.
  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      const target = event.target as HTMLElement | null;
      const typing =
        target != null &&
        (target.tagName === 'INPUT' || target.tagName === 'SELECT' || target.isContentEditable);

      // Undo/redo are the exception: they are modified shortcuts by convention,
      // and they should work while a name field has focus too.
      if ((event.ctrlKey || event.metaKey) && !event.altKey) {
        const key = event.key.toLowerCase();

        if (key === 'z') {
          event.preventDefault();
          applyHistory(event.shiftKey ? redoChange : undoChange);
        } else if (key === 'y') {
          event.preventDefault();
          applyHistory(redoChange);
        }

        return;
      }

      if (typing || event.ctrlKey || event.altKey || event.metaKey) return;

      if (event.key === 'Escape') {
        event.preventDefault();
        toggleMute();
      } else if (event.key === 'b' || event.key === 'B') {
        event.preventDefault();
        toggleBypass();
      }
    };

    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  }, [toggleMute, toggleBypass, applyHistory]);

  const scrollToEffect = useCallback((effectId: string) => {
    document.getElementById(`rack-${effectId}`)?.scrollIntoView({
      behavior: 'smooth',
      block: 'nearest',
    });
  }, []);

  // Global has its own controls in the top bar and the tempo panel; the
  // metronome gets the tempo panel too. Neither belongs in the rack of stages.
  const rackEffects = useMemo(
    () => effects.filter((effect) => effect.id !== 'global' && effect.id !== 'metronome'),
    [effects],
  );

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
          {/* Post-trim, because that is what the Input knob is dialled against
              and what the chain actually receives. The pre-trim figure is still
              passed in so a clipping interface is reported rather than hidden
              behind a trim that pulled the reading back down. */}
          <LevelMeter
            label="Input"
            db={levels.chainInputLevel}
            sourceDb={levels.inputLevel}
          />
          <LevelMeter label="Output" db={levels.outputLevel} />
          <ReductionMeter label="Comp" db={levels.compressorReductionDb} />
          <ReductionMeter label="Limiter" db={levels.limiterReductionDb} />
        </div>

        <div className="topbar__actions">
          <button
            type="button"
            className="pill-btn"
            disabled={offline || !history.canUndo}
            onClick={() => applyHistory(undoChange)}
            title="Batalkan perubahan terakhir (Ctrl+Z)"
            aria-label="Batalkan"
          >
            ↶
          </button>
          <button
            type="button"
            className="pill-btn"
            disabled={offline || !history.canRedo}
            onClick={() => applyHistory(redoChange)}
            title="Ulangi perubahan (Ctrl+Shift+Z)"
            aria-label="Ulangi"
          >
            ↷
          </button>
          {bypass ? (
            <button
              type="button"
              className={`pill-btn${isBypassed ? ' pill-btn--active' : ''}`}
              disabled={offline}
              onClick={toggleBypass}
              title="Bandingkan dengan sinyal kering (B)"
              aria-pressed={isBypassed}
            >
              Bypass
            </button>
          ) : null}
          {masterMuted ? (
            <button
              type="button"
              className={`pill-btn${isMuted ? ' pill-btn--danger' : ''}`}
              disabled={offline}
              onClick={toggleMute}
              title="Bisukan keluaran (Esc)"
              aria-pressed={isMuted}
            >
              {isMuted ? 'Bisu' : 'Mute'}
            </button>
          ) : null}
        </div>

        <div className="topbar__master">
          {masterVolume ? (
            <Knob
              value={Number(masterVolume.value)}
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

      {isBypassed ? (
        <div className="banner banner--warn" role="status">
          Global bypass aktif — kamu mendengar sinyal kering tanpa efek.
        </div>
      ) : null}

      {message ? (
        <div className="banner" role="status">
          {message}
        </div>
      ) : null}

      <ChainStrip
        effects={effects}
        disabled={offline}
        onSelect={scrollToEffect}
        onToggle={(id, enabled) => void handleEnabledChange(id, enabled)}
      />

      <main className="layout">
        <div className="layout__chain">
          {effects.length === 0 && !offline ? (
            <p className="layout__empty">Memuat rantai efek...</p>
          ) : null}

          {rackEffects.map((effect, index) => (
            <EffectRack
              key={effect.id}
              effect={effect}
              index={index + 1}
              total={rackEffects.length}
              disabled={offline}
              sampleRate={levels.sampleRate || undefined}
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

          <TunerDisplay disabled={offline} />

          <TempoPanel
            bpm={bpm}
            metronome={metronome}
            disabled={offline}
            onParameterChange={handleParameterChange}
            onEnabledChange={(id, enabled) => void handleEnabledChange(id, enabled)}
          />

          <SceneGrid
            effects={rackEffects}
            disabled={offline}
            onRecalled={() => void refreshEffects()}
          />

          <PresetControls
            presets={presets}
            details={presetDetails}
            selected={selectedPreset}
            busy={offline}
            onLoad={handlePresetLoad}
            onSave={handlePresetSave}
            onDelete={handlePresetDelete}
            onMetadataChange={handleMetadataChange}
            onExport={handlePresetExport}
            onImport={handlePresetImport}
          />

          <MidiMapping effects={effects} disabled={offline} />

          <NamPanel disabled={offline} onLibraryChanged={() => void refreshEffects()} />

          <section className="panel" aria-label="Impulse response">
            <header className="panel__head">
              <h2 className="panel__title">Impulse Response</h2>
              <button type="button" className="btn btn--ghost" disabled={offline} onClick={handleRevealIr}>
                Buka folder
              </button>
            </header>
            <p className="panel__hint">
              Letakkan berkas WAV di folder <code>Cabinets</code> atau <code>Reverbs</code>, lalu
              pilih pada kartu Cabinet / Reverb. Tanpa berkas, keduanya memakai algoritma bawaan.
            </p>
          </section>

          <section className="panel" aria-label="Performa">
            <header className="panel__head">
              <h2 className="panel__title">Performa</h2>
            </header>

            <Sparkline values={cpuHistory} max={100} label="Riwayat beban DSP" />

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
                <dd>
                  {levels.gateGain > 0.99
                    ? 'Terbuka'
                    : levels.gateGain < 0.01
                      ? 'Tertutup'
                      : 'Menutup'}
                </dd>
              </div>
            </dl>
          </section>
        </aside>
      </main>
    </div>
  );
}

export default App;
