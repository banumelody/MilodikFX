import { memo } from 'react';

import { EFFECT_ACCENTS } from './EffectRack';
import { effectType } from '../services/api';
import type { EffectDescriptor } from '../services/api';
import { useT } from '../i18n';

/**
 * The blocks that are not on the board yet.
 *
 * Modelled on Logic Pro's Pedal Browser and Fractal's block library: the board
 * starts as a straight wire and you fetch what you want. An empty board is not
 * a special state -- the signal simply passes through, the way Fractal's grid
 * crosses an empty space with a passive shunt.
 */
export interface BoardPaletteProps {
  /** Every placeable stage, in chain order, with its `placed` flag. */
  stages: EffectDescriptor[];
  disabled?: boolean;
  /** Pointer/keyboard props for one palette chip, from useChainReorder. */
  paletteProps: (id: string) => Record<string, unknown>;
  /** The id being dragged, so the chip can show it has left the palette. */
  activeId?: string | null;
}

/**
 * Which shelf each block sits on.
 *
 * Grouped by what the block does to the signal rather than alphabetically,
 * because that is how anyone building a rig thinks about the order: dynamics
 * first, then drive, then tone, then the amp, then the room.
 */
const GROUPS: Array<{ title: string; ids: string[] }> = [
  { title: 'Utilitas', ids: ['split', 'mixer'] },
  { title: 'Dinamika', ids: ['noiseGate', 'compressor', 'cleanBoost'] },
  { title: 'Drive', ids: ['overdrive'] },
  { title: 'Nada', ids: ['eq', 'toneStack'] },
  { title: 'Amp', ids: ['nam', 'cabinet'] },
  { title: 'Ruang', ids: ['delay', 'reverb'] },
];

/** "Overdrive 2" is an instance; the palette offers the type, so drop the number. */
const stripInstance = (label: string) => label.replace(/\s+\d+$/, '');

function BoardPaletteBase({ stages, disabled = false, paletteProps, activeId }: BoardPaletteProps) {
  const t = useT();

  // One entry per *type*, carrying how many of it are left. Three separate
  // Overdrive rows would be a list of near-identical entries where the only
  // real question is "have I got another one" -- which the count answers.
  //
  // The Mixer never appears: it arrives with the Splitter and leaves with it,
  // exactly as Apple's does. On its own it would be a mix point with nothing
  // to mix.
  const byType = new Map<string, { next: EffectDescriptor; left: number; total: number }>();

  for (const stage of stages) {
    const type = effectType(stage.id);

    if (type === 'mixer') continue;

    const entry = byType.get(type) ?? { next: stage, left: 0, total: 0 };

    entry.total += 1;

    if (stage.placed === false) {
      // The lowest-numbered free instance, so placing twice gives 2 then 3.
      if (entry.left === 0) entry.next = stage;
      entry.left += 1;
    }

    byType.set(type, entry);
  }

  const offered = [...byType.values()].filter((entry) => entry.left > 0);

  return (
    <section className="panel palette" aria-label={t('board.paletteAria')}>
      <header className="panel__head">
        <h2 className="panel__title">Blok</h2>
      </header>

      {offered.length === 0 ? (
        <p className="panel__empty">{t('board.paletteFull')}</p>
      ) : (
        <>
          <p className="palette__hint">
            {t('board.paletteHint')}
          </p>

          {/* The list scrolls inside itself rather than growing the sidebar. An
              unbounded palette pushes the device panel below the fold, and how
              far depends on how many blocks happen to be unplaced -- so the
              panels underneath would move about for reasons nobody chose. */}
          <div className="palette__list">
            {GROUPS.map((group) => {
              const items = group.ids
                .map((type) => byType.get(type))
                .filter(
                  (entry): entry is NonNullable<typeof entry> => entry != null && entry.left > 0
                );

              if (items.length === 0) return null;

              return (
                <div key={group.title} className="palette__group">
                  <p className="palette__group-title">{group.title}</p>

                  {items.map(({ next, left, total }) => {
                    const type = effectType(next.id);

                    return (
                      <button
                        key={type}
                        type="button"
                        className={`palette__chip${
                          activeId === next.id ? ' palette__chip--dragging' : ''
                        }`}
                        style={{ ['--hue' as string]: EFFECT_ACCENTS[type] ?? '#4da3ff' }}
                        disabled={disabled}
                        data-palette-stage={next.id}
                        title={
                          type === 'split'
                            ? t('board.splitterHint')
                            : next.description
                        }
                        {...paletteProps(next.id)}
                      >
                        <i aria-hidden="true" />
                        {stripInstance(next.label)}
                        {total > 1 ? (
                          <span className="palette__left">
                            {left}/{total}
                          </span>
                        ) : null}
                      </button>
                    );
                  })}
                </div>
              );
            })}
          </div>
        </>
      )}
    </section>
  );
}

export const BoardPalette = memo(BoardPaletteBase);

export default BoardPalette;
