import { memo, useCallback, useEffect, useRef, useState } from 'react';

import { getScenes, recallScene, setTunerEnabled, subscribeTuner } from '../services/api';
import type { Levels, ParameterDescriptor, ScenesState, TunerReading } from '../services/api';

/** Taps further apart than this start a new measurement. */
const TAP_TIMEOUT_MS = 2000;
const TAP_HISTORY = 4;

/** Within this many cents the tuner reads in tune. */
const IN_TUNE_CENTS = 5;

const IDLE_TUNER: TunerReading = {
  enabled: false,
  note: '',
  midiNote: -1,
  frequency: 0,
  cents: 0,
  confidence: 0,
  detected: false,
};

export interface PerformViewProps {
  levels: Levels;
  presets: string[];
  selectedPreset: string;
  onLoadPreset: (name: string) => void;
  bpm?: ParameterDescriptor;
  onParameterChange: (effectId: string, parameterId: string, value: number) => void;
  isBypassed: boolean;
  isMuted: boolean;
  onToggleBypass: () => void;
  onToggleMute: () => void;
  offline: boolean;
  /** Refresh the rack after a scene recall changes the on/off pattern. */
  onScenesRecalled: () => void;
  /** Bumped when a footswitch changed scenes; the big buttons need to catch up. */
  refreshToken?: number;
}

/** A wide LED-style meter, -60..0 dB, for reading across the room. */
function BigMeter({ label, db }: { label: string; db: number }) {
  const segments = 20;
  const lit = Math.round(((Math.max(-60, Math.min(0, db)) + 60) / 60) * segments);

  return (
    <div className="perform__meter">
      <span className="perform__meter-label">{label}</span>
      <div className="perform__meter-track" role="meter" aria-label={label} aria-valuenow={Math.round(db)}>
        {Array.from({ length: segments }, (_, i) => {
          const level = i < 12 ? 'ok' : i < 17 ? 'warn' : 'hot';
          return (
            <span
              key={i}
              className={`perform__seg perform__seg--${level}${i < lit ? ' perform__seg--on' : ''}`}
            />
          );
        })}
      </div>
    </div>
  );
}

/**
 * The stage-facing screen: big, few controls, readable at arm's length while
 * holding a guitar. Everything here already exists in Edit; this is the same
 * engine seen from the "perform" posture rather than the "tweak" one.
 */
function PerformViewBase({
  levels,
  presets,
  selectedPreset,
  onLoadPreset,
  bpm,
  onParameterChange,
  isBypassed,
  isMuted,
  onToggleBypass,
  onToggleMute,
  offline,
  onScenesRecalled,
  refreshToken,
}: PerformViewProps) {
  const [scenes, setScenes] = useState<ScenesState | null>(null);
  const [tunerOn, setTunerOn] = useState(false);
  const [reading, setReading] = useState<TunerReading>(IDLE_TUNER);

  const taps = useRef<number[]>([]);
  const inFlight = useRef(false);
  const recalledRef = useRef(onScenesRecalled);
  recalledRef.current = onScenesRecalled;

  const refreshScenes = useCallback(async () => {
    try {
      setScenes(await getScenes());
    } catch {
      /* the connection banner already reports an unreachable engine */
    }
  }, []);

  useEffect(() => {
    void refreshScenes();
  }, [refreshScenes, refreshToken]);

  const recall = useCallback(
    (index: number) => {
      if (inFlight.current || offline) return;
      inFlight.current = true;
      void (async () => {
        try {
          setScenes(await recallScene(index));
          recalledRef.current();
        } catch {
          /* ignore; banner covers it */
        } finally {
          inFlight.current = false;
        }
      })();
    },
    [offline],
  );

  const changePreset = useCallback(
    (delta: number) => {
      if (presets.length === 0) return;
      const current = presets.indexOf(selectedPreset);
      const next = (current < 0 ? 0 : current + delta + presets.length) % presets.length;
      onLoadPreset(presets[next]);
    },
    [presets, selectedPreset, onLoadPreset],
  );

  const tap = useCallback(() => {
    if (!bpm || offline) return;

    const now = performance.now();
    const history = taps.current;
    if (history.length > 0 && now - history[history.length - 1] > TAP_TIMEOUT_MS) history.length = 0;
    history.push(now);
    if (history.length > TAP_HISTORY + 1) history.shift();
    if (history.length < 2) return;

    const intervals: number[] = [];
    for (let i = 1; i < history.length; i += 1) intervals.push(history[i] - history[i - 1]);
    const average = intervals.reduce((sum, v) => sum + v, 0) / intervals.length;
    if (average <= 0) return;

    const tapped = Math.round(60000 / average);
    onParameterChange('global', 'bpm', Math.min(bpm.max, Math.max(bpm.min, tapped)));
  }, [bpm, offline, onParameterChange]);

  // Tuner subscription, only while it is switched on here.
  useEffect(() => {
    if (!tunerOn) return undefined;

    let cancelled = false;
    void setTunerEnabled(true).catch(() => {});

    const stop = subscribeTuner(
      (next) => {
        if (!cancelled) setReading(next);
      },
      () => {
        if (!cancelled) setReading(IDLE_TUNER);
      },
    );

    return () => {
      cancelled = true;
      stop();
      void setTunerEnabled(false).catch(() => {});
      setReading(IDLE_TUNER);
    };
  }, [tunerOn]);

  // Perform-only keys: number keys recall scenes, arrows step presets, T taps.
  // Esc (mute) and B (bypass) stay global, handled in App.
  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      const el = event.target as HTMLElement | null;
      if (el && (el.tagName === 'INPUT' || el.tagName === 'SELECT' || el.isContentEditable)) return;
      if (event.ctrlKey || event.metaKey || event.altKey) return;

      if (event.key >= '1' && event.key <= '4') {
        event.preventDefault();
        recall(Number(event.key) - 1);
      } else if (event.key === 'ArrowRight') {
        event.preventDefault();
        changePreset(1);
      } else if (event.key === 'ArrowLeft') {
        event.preventDefault();
        changePreset(-1);
      } else if (event.key === 't' || event.key === 'T') {
        event.preventDefault();
        tap();
      }
    };

    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [recall, changePreset, tap]);

  const tempo = Math.round(Number(bpm?.value ?? 120));
  const detected = tunerOn && reading.detected;
  const inTune = detected && Math.abs(reading.cents) <= IN_TUNE_CENTS;
  const tunerState = !detected ? 'idle' : inTune ? 'in-tune' : reading.cents < 0 ? 'flat' : 'sharp';
  const sceneList = scenes?.scenes ?? [];

  return (
    <div className="perform" aria-label="Mode Perform">
      <div className="perform__top">
        <div className="perform__preset">
          <button
            type="button"
            className="perform__nav"
            aria-label="Preset sebelumnya"
            disabled={offline || presets.length === 0}
            onClick={() => changePreset(-1)}
          >
            ‹
          </button>
          <div className="perform__preset-name" title={selectedPreset || 'Belum ada preset'}>
            {selectedPreset || '—'}
          </div>
          <button
            type="button"
            className="perform__nav"
            aria-label="Preset berikutnya"
            disabled={offline || presets.length === 0}
            onClick={() => changePreset(1)}
          >
            ›
          </button>
        </div>

        <div className="perform__tempo">
          <output className="perform__bpm" aria-label="Tempo">
            {tempo}
            <span className="perform__bpm-unit">BPM</span>
          </output>
          <button type="button" className="perform__tap" disabled={offline || !bpm} onClick={tap}>
            Tap
          </button>
        </div>
      </div>

      {tunerOn ? (
        <div className={`perform__tuner perform__tuner--${tunerState}`} role="status" aria-live="polite">
          <span className="perform__tuner-note">{detected ? reading.note : '—'}</span>
          <div className="perform__tuner-scale">
            <span className="perform__tuner-centre" />
            {detected ? (
              <span
                className="perform__tuner-needle"
                style={{ left: `${50 + Math.max(-50, Math.min(50, reading.cents))}%` }}
              />
            ) : null}
          </div>
          <span className="perform__tuner-cents">
            {detected ? `${reading.cents > 0 ? '+' : ''}${reading.cents.toFixed(0)} ¢` : 'Petik satu senar'}
          </span>
        </div>
      ) : (
        <div className="perform__scenes" role="group" aria-label="Scene">
          {sceneList.map((scene) => (
            <button
              key={scene.index}
              type="button"
              className={`perform__scene${scenes?.active === scene.index ? ' perform__scene--active' : ''}`}
              disabled={offline}
              aria-pressed={scenes?.active === scene.index}
              onClick={() => recall(scene.index)}
            >
              <span className="perform__scene-num">{scene.index + 1}</span>
              <span className="perform__scene-name">{scene.name}</span>
            </button>
          ))}
        </div>
      )}

      <div className="perform__bottom">
        <div className="perform__meters">
          <BigMeter label="In" db={levels.chainInputLevel} />
          <BigMeter label="Out" db={levels.outputLevel} />
        </div>

        <div className="perform__switches">
          <button
            type="button"
            className={`perform__switch${tunerOn ? ' perform__switch--active' : ''}`}
            disabled={offline}
            aria-pressed={tunerOn}
            onClick={() => setTunerOn((on) => !on)}
          >
            Tuner
          </button>
          <button
            type="button"
            className={`perform__switch${isBypassed ? ' perform__switch--warn' : ''}`}
            disabled={offline}
            aria-pressed={isBypassed}
            onClick={onToggleBypass}
          >
            Bypass
          </button>
          <button
            type="button"
            className={`perform__switch${isMuted ? ' perform__switch--danger' : ''}`}
            disabled={offline}
            aria-pressed={isMuted}
            onClick={onToggleMute}
          >
            {isMuted ? 'Bisu' : 'Mute'}
          </button>
        </div>
      </div>

      <p className="perform__hint">
        Angka <kbd>1</kbd>–<kbd>4</kbd> pindah scene · <kbd>←</kbd> <kbd>→</kbd> ganti preset ·{' '}
        <kbd>T</kbd> tap tempo · <kbd>Esc</kbd> bisu · <kbd>B</kbd> bypass
      </p>
    </div>
  );
}

export const PerformView = memo(PerformViewBase);

export default PerformView;
