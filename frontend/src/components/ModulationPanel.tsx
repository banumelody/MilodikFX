import { memo, useCallback, useEffect, useMemo, useRef, useState } from 'react';

import { clearModifier, getModifiers, setModifier } from '../services/api';
import type { EffectDescriptor, ModifiersState, ModifierSource } from '../services/api';

const SOURCES: { value: ModifierSource; label: string }[] = [
  { value: 'lfoSine', label: 'LFO Sinus' },
  { value: 'lfoTriangle', label: 'LFO Segitiga' },
  { value: 'lfoSquare', label: 'LFO Kotak' },
  { value: 'envelope', label: 'Envelope (dinamika)' },
  { value: 'expression', label: 'Pedal ekspresi' },
];

/** Note divisions an LFO can lock to the tempo. Index matches the engine's enum. */
const SYNC_DIVISIONS = ['Bebas', '1/1', '1/2', '1/4', '1/8', '1/8.', '1/8T', '1/16'];

interface Target {
  key: string;
  effect: string;
  parameter: string;
  label: string;
  min: number;
  max: number;
  unit: string;
}

/** Only numeric parameters can be swept -- a switch or a filename cannot. */
function collectTargets(effects: EffectDescriptor[]): Target[] {
  const result: Target[] = [];

  for (const effect of effects) {
    for (const parameter of effect.parameters) {
      if (parameter.type !== 'float') continue;

      result.push({
        key: `${effect.id}.${parameter.id}`,
        effect: effect.id,
        parameter: parameter.id,
        label: `${effect.label} — ${parameter.label}`,
        min: parameter.min,
        max: parameter.max,
        unit: parameter.unit,
      });
    }
  }

  return result;
}

interface ModulationPanelProps {
  effects: EffectDescriptor[];
  disabled?: boolean;
  /** Fired after a modifier is added or removed, so the rack can re-read which
      knobs are now owned by a modifier. */
  onModifiersChanged: () => void;
}

/**
 * Modifiers: an LFO or the envelope of your picking sweeps one parameter between
 * two values. A tremolo is the master level swept by an LFO; an auto-wah is the
 * contour frequency swept by the envelope. Up to four at once.
 */
function ModulationPanelBase({ effects, disabled = false, onModifiersChanged }: ModulationPanelProps) {
  const [state, setState] = useState<ModifiersState | null>(null);
  const [error, setError] = useState<string | null>(null);

  const targets = useMemo(() => collectTargets(effects), [effects]);
  const byKey = useMemo(() => new Map(targets.map((t) => [t.key, t])), [targets]);

  const [targetKey, setTargetKey] = useState('');
  const [source, setSource] = useState<ModifierSource>('lfoSine');
  const [low, setLow] = useState(0);
  const [high, setHigh] = useState(100);
  const [rate, setRate] = useState(2);
  const [syncDivision, setSyncDivision] = useState(0);
  const [expressionCc, setExpressionCc] = useState(11);

  const inFlight = useRef(false);
  const changedRef = useRef(onModifiersChanged);
  changedRef.current = onModifiersChanged;

  const run = useCallback(async (action: () => Promise<ModifiersState>, notify = false) => {
    if (inFlight.current) return;
    inFlight.current = true;
    try {
      setState(await action());
      setError(null);
      if (notify) changedRef.current();
    } catch (caught) {
      setError(caught instanceof Error ? caught.message : String(caught));
    } finally {
      inFlight.current = false;
    }
  }, []);

  useEffect(() => {
    void run(getModifiers);
  }, [run]);

  // Picking a target seeds the sweep to its full range, the most useful default.
  const onPickTarget = useCallback(
    (key: string) => {
      setTargetKey(key);
      const target = byKey.get(key);
      if (target) {
        setLow(target.min);
        setHigh(target.max);
      }
    },
    [byKey],
  );

  const modifiers = state?.modifiers ?? [];
  const active = modifiers.filter((m) => m.active);
  const freeSlot = modifiers.find((m) => !m.active)?.slot ?? -1;
  const busy = disabled || state == null;

  const isLfo = source !== 'envelope' && source !== 'expression';
  const isExpression = source === 'expression';
  const selected = byKey.get(targetKey);

  const add = useCallback(() => {
    const target = byKey.get(targetKey);
    if (!target || freeSlot < 0) return;

    void run(
      () =>
        setModifier(freeSlot, {
          effect: target.effect,
          parameter: target.parameter,
          source,
          low,
          high,
          rateHz: rate,
          syncDivision: isLfo ? syncDivision : 0,
          expressionCc: source === 'expression' ? expressionCc : -1,
        }),
      true,
    );
  }, [byKey, targetKey, freeSlot, source, low, high, rate, syncDivision, expressionCc, isLfo, run]);

  return (
    <section className="panel" aria-label="Modifier">
      <header className="panel__head">
        <h2 className="panel__title">Modifier</h2>
        <span className="panel__count">{active.length}/4</span>
      </header>

      <p className="panel__hint">
        LFO, envelope, atau pedal ekspresi menyapu satu parameter antara dua nilai — tremolo,
        auto-wah, wah. LFO bisa dikunci ke tempo. Knob yang dimodulasi tetap aktif: ia menyetel titik
        tengah sapuan (tag <b>MOD</b>); hapus modifier untuk melepaskannya.
      </p>

      {active.length > 0 ? (
        <ul className="midi__list">
          {active.map((modifier) => (
            <li key={modifier.slot} className="midi__item">
              <span className="midi__target">
                {byKey.get(`${modifier.effect}.${modifier.parameter}`)?.label ??
                  `${modifier.effect} — ${modifier.parameter}`}
              </span>
              <span className="pill">{SOURCES.find((s) => s.value === modifier.source)?.label ?? modifier.source}</span>
              <button
                type="button"
                className="btn btn--ghost"
                aria-label={`Hapus modifier ${modifier.slot}`}
                disabled={busy}
                onClick={() => void run(() => clearModifier(modifier.slot), true)}
              >
                Hapus
              </button>
            </li>
          ))}
        </ul>
      ) : null}

      {freeSlot >= 0 ? (
        <div className="modform">
          <label className="field">
            <span className="field__label">Parameter</span>
            <select
              aria-label="Parameter"
              value={targetKey}
              disabled={busy}
              onChange={(event) => onPickTarget(event.target.value)}
            >
              <option value="">Pilih parameter...</option>
              {targets.map((target) => (
                <option key={target.key} value={target.key}>
                  {target.label}
                </option>
              ))}
            </select>
          </label>

          <label className="field">
            <span className="field__label">Sumber</span>
            <select
              aria-label="Sumber"
              value={source}
              disabled={busy}
              onChange={(event) => setSource(event.target.value as ModifierSource)}
            >
              {SOURCES.map((option) => (
                <option key={option.value} value={option.value}>
                  {option.label}
                </option>
              ))}
            </select>
          </label>

          <div className="modform__range">
            <label className="field field--inline">
              <span className="field__label">Dari</span>
              <input
                type="number"
                aria-label="Dari"
                value={low}
                min={selected?.min}
                max={selected?.max}
                disabled={busy || !selected}
                onChange={(event) => setLow(Number(event.target.value))}
              />
            </label>
            <label className="field field--inline">
              <span className="field__label">Sampai</span>
              <input
                type="number"
                aria-label="Sampai"
                value={high}
                min={selected?.min}
                max={selected?.max}
                disabled={busy || !selected}
                onChange={(event) => setHigh(Number(event.target.value))}
              />
            </label>
            {isLfo ? (
              <label className="field field--inline">
                <span className="field__label">Sync</span>
                <select
                  aria-label="Sync"
                  value={syncDivision}
                  disabled={busy}
                  onChange={(event) => setSyncDivision(Number(event.target.value))}
                >
                  {SYNC_DIVISIONS.map((label, index) => (
                    <option key={label} value={index}>
                      {label}
                    </option>
                  ))}
                </select>
              </label>
            ) : null}
            {isLfo && syncDivision === 0 ? (
              <label className="field field--inline">
                <span className="field__label">Rate (Hz)</span>
                <input
                  type="number"
                  aria-label="Rate (Hz)"
                  value={rate}
                  min={0.05}
                  max={20}
                  step={0.1}
                  disabled={busy}
                  onChange={(event) => setRate(Number(event.target.value))}
                />
              </label>
            ) : null}
            {isExpression ? (
              <label className="field field--inline">
                <span className="field__label">CC pedal</span>
                <input
                  type="number"
                  aria-label="CC pedal"
                  value={expressionCc}
                  min={0}
                  max={127}
                  step={1}
                  disabled={busy}
                  onChange={(event) => setExpressionCc(Number(event.target.value))}
                />
              </label>
            ) : null}
          </div>

          <button type="button" className="btn" disabled={busy || !selected} onClick={add}>
            Tambah modifier
          </button>
        </div>
      ) : (
        <p className="panel__hint">Keempat slot terpakai. Hapus salah satu untuk menambah yang baru.</p>
      )}

      {error ? (
        <p className="panel__error" role="alert">
          {error}
        </p>
      ) : null}
    </section>
  );
}

export const ModulationPanel = memo(ModulationPanelBase);

export default ModulationPanel;
