import { memo, useCallback, useEffect, useRef, useState } from 'react';

import type { EffectDescriptor, ParameterDescriptor } from '../services/api';

/** Taps further apart than this start a new measurement rather than extending one. */
const TAP_TIMEOUT_MS = 2000;

/** How many intervals the average is taken over. */
const TAP_HISTORY = 4;

interface TempoPanelProps {
  bpm?: ParameterDescriptor;
  metronome?: EffectDescriptor;
  disabled?: boolean;
  onParameterChange: (effectId: string, parameterId: string, value: number) => void;
  onEnabledChange: (effectId: string, enabled: boolean) => void;
}

function findParameter(effect: EffectDescriptor | undefined, id: string) {
  return effect?.parameters.find((parameter) => parameter.id === id);
}

/**
 * One tempo for the whole app: the metronome clicks at it and a synced delay
 * takes its repeat time from it.
 *
 * The beat lights run off a local clock anchored to the moment the click was
 * switched on. They are an indicator, not a measurement -- the engine counts
 * beats on the audio clock, and polling it often enough to drive a smooth flash
 * would cost more requests than the flash is worth.
 */
function TempoPanelBase({
  bpm,
  metronome,
  disabled = false,
  onParameterChange,
  onEnabledChange,
}: TempoPanelProps) {
  const volume = findParameter(metronome, 'volumePct');
  const beatsPerBar = findParameter(metronome, 'beatsPerBar');

  const running = metronome?.enabled ?? false;
  const bars = Math.max(1, Math.round(Number(beatsPerBar?.value ?? 4)));
  const tempo = Number(bpm?.value ?? 120);

  const [beat, setBeat] = useState(0);
  const taps = useRef<number[]>([]);

  useEffect(() => {
    if (!running || tempo <= 0) {
      setBeat(0);
      return undefined;
    }

    const started = performance.now();
    const beatMs = 60000 / tempo;
    let frame = 0;

    // Derived from elapsed time rather than incremented per tick, so a dropped
    // frame does not put the lights permanently behind the click.
    const tick = () => {
      setBeat(Math.floor((performance.now() - started) / beatMs) % bars);
      frame = requestAnimationFrame(tick);
    };

    frame = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(frame);
  }, [running, tempo, bars]);

  const handleTap = useCallback(() => {
    if (disabled || !bpm) return;

    const now = performance.now();
    const history = taps.current;

    if (history.length > 0 && now - history[history.length - 1] > TAP_TIMEOUT_MS) history.length = 0;

    history.push(now);

    if (history.length > TAP_HISTORY + 1) history.shift();

    if (history.length < 2) return;

    const intervals: number[] = [];

    for (let i = 1; i < history.length; i += 1) intervals.push(history[i] - history[i - 1]);

    const average = intervals.reduce((sum, value) => sum + value, 0) / intervals.length;

    if (average <= 0) return;

    const tapped = Math.round(60000 / average);
    const clamped = Math.min(bpm.max, Math.max(bpm.min, tapped));

    onParameterChange('global', 'bpm', clamped);
  }, [disabled, bpm, onParameterChange]);

  if (!bpm && !metronome) return null;

  return (
    <section className="panel tempo" aria-label="Tempo">
      <header className="panel__head">
        <h2 className="panel__title">Tempo</h2>
        {metronome ? (
          <button
            type="button"
            className={`pill-btn${running ? ' pill-btn--active' : ''}`}
            disabled={disabled}
            aria-pressed={running}
            aria-label="Metronom"
            onClick={() => onEnabledChange('metronome', !running)}
          >
            {running ? 'Klik aktif' : 'Klik'}
          </button>
        ) : null}
      </header>

      {bpm ? (
        <div className="tempo__row">
          <output className="tempo__bpm" aria-label="Tempo dalam BPM">
            {Math.round(tempo)}
            <span className="tempo__unit">BPM</span>
          </output>

          <button type="button" className="btn tempo__tap" disabled={disabled} onClick={handleTap}>
            Tap
          </button>
        </div>
      ) : null}

      {bpm ? (
        <input
          type="range"
          className="tempo__slider"
          aria-label="Atur tempo"
          min={bpm.min}
          max={bpm.max}
          step={bpm.step}
          value={tempo}
          disabled={disabled}
          onChange={(event) => onParameterChange('global', 'bpm', Number(event.target.value))}
        />
      ) : null}

      {metronome ? (
        <div className="tempo__beats" aria-hidden="true">
          {Array.from({ length: bars }, (_, index) => (
            <span
              key={index}
              className={[
                'tempo__beat',
                index === 0 ? 'tempo__beat--accent' : '',
                running && index === beat ? 'tempo__beat--on' : '',
              ]
                .filter(Boolean)
                .join(' ')}
            />
          ))}
        </div>
      ) : null}

      {beatsPerBar ? (
        <label className="field">
          <span className="field__label">Ketukan per bar</span>
          <select
            value={String(Math.round(Number(beatsPerBar.value)))}
            disabled={disabled}
            onChange={(event) =>
              onParameterChange('metronome', 'beatsPerBar', Number(event.target.value))
            }
          >
            {Array.from({ length: beatsPerBar.max - beatsPerBar.min + 1 }, (_, index) => {
              const value = beatsPerBar.min + index;
              return (
                <option key={value} value={value}>
                  {value}
                </option>
              );
            })}
          </select>
        </label>
      ) : null}

      {volume ? (
        <label className="field">
          <span className="field__label">Volume klik</span>
          <input
            type="range"
            min={volume.min}
            max={volume.max}
            step={volume.step}
            value={Number(volume.value)}
            disabled={disabled}
            onChange={(event) =>
              onParameterChange('metronome', 'volumePct', Number(event.target.value))
            }
          />
        </label>
      ) : null}
    </section>
  );
}

export const TempoPanel = memo(TempoPanelBase);

export default TempoPanel;
