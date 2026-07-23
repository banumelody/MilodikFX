import { memo, useCallback, useEffect, useMemo, useRef, useState } from 'react';

import {
  clearMidiMapping,
  getMidi,
  learnMidi,
  setMidiDevice,
} from '../services/api';
import type {
  EffectDescriptor,
  MidiMapping as MidiMappingEntry,
  MidiState,
  MidiTarget,
} from '../services/api';

/** How often the panel re-reads MIDI state while learn is armed. */
const LEARN_POLL_MS = 250;

/** Scene slots this build has; matches SceneManager::kNumScenes. */
const NUM_SCENES = 4;

interface MidiMappingProps {
  effects: EffectDescriptor[];
  disabled?: boolean;
}

interface Assignable {
  key: string;
  label: string;
  target: MidiTarget;
}

/** A stable key for a mapping, so the list can look its label back up. */
function keyForMapping(mapping: {
  kind?: string;
  effect?: string;
  parameter?: string;
  index?: number;
}): string {
  if (mapping.kind === 'scene') return `scene.${mapping.index}`;
  if (mapping.kind === 'channel') return `channel.${mapping.effect}.${mapping.index}`;
  return `param.${mapping.effect}.${mapping.parameter}`;
}

/**
 * Everything a controller can be bound to.
 *
 * Scenes come first -- a footswitch under each scene is the whole point of a
 * floor rig. Then each block's on/off switch and its channels (A/B/C/D), and
 * finally its parameters. Text parameters are left out -- nothing sensible maps
 * 0..127 onto a list of filenames.
 */
function collectAssignable(effects: EffectDescriptor[]): Assignable[] {
  const result: Assignable[] = [];

  for (let i = 0; i < NUM_SCENES; i += 1) {
    result.push({
      key: `scene.${i}`,
      label: `Scene ${i + 1}`,
      target: { kind: 'scene', index: i, mode: 'toggle' },
    });
  }

  for (const effect of effects) {
    if (effect.toggleable !== false) {
      result.push({
        key: `param.${effect.id}.enabled`,
        label: `${effect.label} — On/Off`,
        target: { kind: 'parameter', effect: effect.id, parameter: 'enabled', mode: 'toggle' },
      });
    }

    if (effect.toggleable !== false && Array.isArray(effect.channels)) {
      effect.channels.forEach((name, i) => {
        result.push({
          key: `channel.${effect.id}.${i}`,
          label: `${effect.label} — Channel ${name}`,
          target: { kind: 'channel', effect: effect.id, index: i, mode: 'toggle' },
        });
      });
    }

    for (const parameter of effect.parameters) {
      if (parameter.type === 'text') continue;

      result.push({
        key: `param.${effect.id}.${parameter.id}`,
        label: `${effect.label} — ${parameter.label}`,
        target: {
          kind: 'parameter',
          effect: effect.id,
          parameter: parameter.id,
          mode: parameter.type === 'bool' ? 'toggle' : 'continuous',
        },
      });
    }
  }

  return result;
}

function MidiMappingBase({ effects, disabled = false }: MidiMappingProps) {
  const [state, setState] = useState<MidiState | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [target, setTarget] = useState('');

  const assignable = useMemo(() => collectAssignable(effects), [effects]);
  const byKey = useMemo(
    () => new Map(assignable.map((item) => [item.key, item])),
    [assignable],
  );

  const inFlight = useRef(false);

  const run = useCallback(async (action: () => Promise<MidiState>) => {
    if (inFlight.current) return;
    inFlight.current = true;

    try {
      setState(await action());
      setError(null);
    } catch (caught) {
      setError(caught instanceof Error ? caught.message : String(caught));
    } finally {
      inFlight.current = false;
    }
  }, []);

  useEffect(() => {
    void run(getMidi);
  }, [run]);

  // Only while learn is armed: the binding happens in the engine when a control
  // moves, so there is nothing else that would tell the panel it landed.
  const learning = state?.learning ?? null;

  useEffect(() => {
    if (!learning) return undefined;

    const timer = window.setInterval(() => void run(getMidi), LEARN_POLL_MS);
    return () => window.clearInterval(timer);
  }, [learning, run]);

  const describe = useCallback(
    (mapping: Pick<MidiMappingEntry, 'kind' | 'effect' | 'parameter' | 'index'>) => {
      const key = keyForMapping(mapping);
      const known = byKey.get(key)?.label;
      if (known) return known;

      // A mapping whose target has since gone (an effect renamed, say) still
      // needs a readable line rather than a blank one.
      if (mapping.kind === 'scene') return `Scene ${(mapping.index ?? 0) + 1}`;
      if (mapping.kind === 'channel') return `${mapping.effect} — Channel ${mapping.index}`;
      return `${mapping.effect} — ${mapping.parameter}`;
    },
    [byKey],
  );

  const startLearn = useCallback(() => {
    const chosen = byKey.get(target);
    if (!chosen) return;

    void run(() => learnMidi(chosen.target));
  }, [byKey, target, run]);

  const busy = disabled || state == null;
  const mappings = state?.mappings ?? [];

  return (
    <section className="panel midi" aria-label="MIDI">
      <header className="panel__head">
        <h2 className="panel__title">MIDI</h2>
        <button
          type="button"
          className="btn btn--ghost"
          disabled={disabled}
          onClick={() => void run(getMidi)}
        >
          Muat ulang
        </button>
      </header>

      <label className="field">
        <span className="field__label">Perangkat</span>
        <select
          aria-label="Perangkat"
          value={state?.current ?? ''}
          disabled={busy}
          onChange={(event) => void run(() => setMidiDevice(event.target.value))}
        >
          <option value="">Tidak dipakai</option>
          {(state?.devices ?? []).map((device) => (
            <option key={device} value={device}>
              {device}
            </option>
          ))}
          {/* A controller unplugged since last run still has its mappings, and
              they must not silently disappear from the list. */}
          {state?.current && !(state.devices ?? []).includes(state.current) ? (
            <option value={state.current}>{state.current} (tidak terhubung)</option>
          ) : null}
        </select>
      </label>

      {state && state.devices.length === 0 ? (
        <p className="panel__hint">
          Tidak ada perangkat MIDI terdeteksi. Colokkan footswitch atau controller, lalu tekan Muat
          ulang.
        </p>
      ) : null}

      <label className="field">
        <span className="field__label">Pasang kontrol ke</span>
        <select
          aria-label="Pasang kontrol ke"
          value={target}
          disabled={busy || !state?.open}
          onChange={(event) => setTarget(event.target.value)}
        >
          <option value="">Pilih tujuan...</option>
          {assignable.map((item) => (
            <option key={item.key} value={item.key}>
              {item.label}
            </option>
          ))}
        </select>
      </label>

      <div className="midi__actions">
        <button
          type="button"
          className={`btn${learning ? ' btn--danger' : ''}`}
          disabled={busy || !state?.open || (!learning && target === '')}
          onClick={() => (learning ? void run(() => learnMidi()) : startLearn())}
        >
          {learning ? 'Batal' : 'MIDI Learn'}
        </button>

        {state && state.lastCc >= 0 ? (
          <span className="midi__last">
            Terakhir: CC {state.lastCc} = {state.lastValue}
          </span>
        ) : null}
      </div>

      {learning ? (
        <p className="panel__hint" role="status">
          Menunggu... gerakkan knob atau injak footswitch untuk memasangnya ke{' '}
          <strong>{describe(learning)}</strong>.
        </p>
      ) : null}

      {mappings.length > 0 ? (
        <ul className="midi__list">
          {mappings.map((mapping) => (
            <li key={mapping.cc} className="midi__item">
              <span className="midi__cc">CC {mapping.cc}</span>
              <span className="midi__target">{describe(mapping)}</span>
              <span className="pill">{mapping.mode === 'toggle' ? 'Saklar' : 'Kontinu'}</span>
              <button
                type="button"
                className="btn btn--ghost"
                aria-label={`Hapus CC ${mapping.cc}`}
                disabled={busy}
                onClick={() => void run(() => clearMidiMapping(mapping.cc))}
              >
                Hapus
              </button>
            </li>
          ))}
        </ul>
      ) : (
        <p className="panel__hint">
          Belum ada kontrol terpasang. Pilih parameter di atas, tekan MIDI Learn, lalu gerakkan
          kontrol fisiknya. Program Change memuat preset sesuai urutan daftar preset.
        </p>
      )}

      {error ? (
        <p className="panel__error" role="alert">
          {error}
        </p>
      ) : null}
    </section>
  );
}

export const MidiMapping = memo(MidiMappingBase);

export default MidiMapping;
