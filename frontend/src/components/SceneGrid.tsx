import { useCallback, useEffect, useRef, useState } from 'react';

import { EFFECT_ACCENTS } from './EffectRack';
import { captureScene, getScenes, recallScene, renameScene, setSceneEffect } from '../services/api';
import type { EffectDescriptor, ScenesState } from '../services/api';

interface SceneGridProps {
  effects: EffectDescriptor[];
  disabled?: boolean;
  /** Called after a recall, so the rack can pick up the new on/off pattern. */
  onRecalled: () => void;
}

/**
 * Four on/off snapshots, as a grid of scene rows against effect columns.
 *
 * Scenes only carry which effects are on, never knob values -- see
 * SceneManager for why. Clicking a cell edits the stored scene without
 * touching what is playing; the number button recalls it.
 */
export function SceneGrid({ effects, disabled = false, onRecalled }: SceneGridProps) {
  const [state, setState] = useState<ScenesState | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [editing, setEditing] = useState<number | null>(null);

  const inFlight = useRef(false);

  // Held in a ref rather than closed over: the caller passes a fresh arrow
  // function on every render, and depending on it made `run` change identity
  // every render too -- which re-ran the mount effect, refetched continuously,
  // and left the in-flight guard permanently set so clicks did nothing.
  const recalledRef = useRef(onRecalled);
  recalledRef.current = onRecalled;

  const run = useCallback(async (action: () => Promise<ScenesState>, recalled = false) => {
    if (inFlight.current) return;
    inFlight.current = true;

    try {
      setState(await action());
      setError(null);
      if (recalled) recalledRef.current();
    } catch (caught) {
      setError(caught instanceof Error ? caught.message : String(caught));
    } finally {
      inFlight.current = false;
    }
  }, []);

  useEffect(() => {
    void run(getScenes);
  }, [run]);

  // Only the effects a scene can actually switch. A stage that is always in
  // the path has no flag to store, so a column for it would never do anything.
  const columns = effects.filter((effect) => effect.toggleable !== false);

  if (columns.length === 0) return null;

  const scenes = state?.scenes ?? [];
  const busy = disabled || state == null;

  return (
    <section className="panel scenes" aria-label="Scene">
      <header className="panel__head">
        <h2 className="panel__title">Scene</h2>
        {state && state.active < 0 ? (
          <span className="panel__count">Diubah manual</span>
        ) : null}
      </header>

      <p className="panel__hint">
        Scene hanya menyimpan efek mana yang menyala — knob tidak ikut berpindah. Klik nomor untuk
        pindah, klik sel untuk mengubah pola, klik nama untuk mengganti namanya.
      </p>

      <div className="scenes__grid" role="table" aria-label="Pola efek per scene">
        <div className="scenes__row scenes__row--head" role="row">
          <span className="scenes__corner" role="columnheader" aria-label="Scene" />
          {columns.map((effect) => (
            <abbr
              key={effect.id}
              className="scenes__col"
              role="columnheader"
              title={effect.label}
              style={{ '--accent': EFFECT_ACCENTS[effect.id] ?? '#4da3ff' } as React.CSSProperties}
            >
              {effect.label.slice(0, 2)}
            </abbr>
          ))}
        </div>

        {scenes.map((scene) => (
          <div className="scenes__row" role="row" key={scene.index}>
            {editing === scene.index ? (
              <input
                className="scenes__rename"
                aria-label={`Nama scene ${scene.index + 1}`}
                defaultValue={scene.name}
                autoFocus
                onBlur={(event) => {
                  setEditing(null);
                  const next = event.target.value.trim();
                  if (next && next !== scene.name) void run(() => renameScene(scene.index, next));
                }}
                onKeyDown={(event) => {
                  if (event.key === 'Enter') event.currentTarget.blur();
                  if (event.key === 'Escape') setEditing(null);
                }}
              />
            ) : (
              <button
                type="button"
                className={`scenes__name${state?.active === scene.index ? ' scenes__name--active' : ''}`}
                role="rowheader"
                disabled={busy}
                aria-pressed={state?.active === scene.index}
                title={
                  scene.populated
                    ? `Pindah ke ${scene.name}`
                    : `${scene.name} masih kosong — simpan dulu`
                }
                onClick={() => void run(() => recallScene(scene.index), true)}
                onDoubleClick={() => setEditing(scene.index)}
              >
                <span className="scenes__number">{scene.index + 1}</span>
                <span className="scenes__label">{scene.name}</span>
              </button>
            )}

            {columns.map((effect) => {
              const on = scene.enabled[effect.id] ?? false;

              return (
                <button
                  key={effect.id}
                  type="button"
                  role="cell"
                  className={`scenes__cell${on ? ' scenes__cell--on' : ''}`}
                  style={{ '--accent': EFFECT_ACCENTS[effect.id] ?? '#4da3ff' } as React.CSSProperties}
                  disabled={busy}
                  aria-pressed={on}
                  aria-label={`${scene.name}: ${effect.label} ${on ? 'menyala' : 'mati'}`}
                  onClick={() => void run(() => setSceneEffect(scene.index, effect.id, !on))}
                />
              );
            })}

            <button
              type="button"
              className="btn btn--ghost scenes__store"
              disabled={busy}
              aria-label={`Rekam kondisi sekarang ke ${scene.name}`}
              title="Rekam pola efek yang sedang aktif ke scene ini"
              onClick={() => void run(() => captureScene(scene.index))}
            >
              {/* Not "Simpan": that is what the preset panel's button says, and
                  two buttons a few centimetres apart both reading "Simpan" but
                  writing different things is a trap. */}
              Rekam
            </button>
          </div>
        ))}
      </div>

      {error ? (
        <p className="panel__error" role="alert">
          {error}
        </p>
      ) : null}
    </section>
  );
}

export default SceneGrid;
