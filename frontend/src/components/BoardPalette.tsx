import { memo } from 'react';

import { EFFECT_ACCENTS } from './EffectRack';
import type { EffectDescriptor } from '../services/api';

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

function BoardPaletteBase({ stages, disabled = false, paletteProps, activeId }: BoardPaletteProps) {
  const unplaced = stages.filter((stage) => stage.placed === false);

  // The Mixer is never fetched by hand: it arrives with the Splitter and leaves
  // with it, exactly as Apple's does. Offering it on its own would let someone
  // build a board with a mix point and nothing to mix.
  const offered = unplaced.filter((stage) => stage.id !== 'mixer');

  const byId = new Map(offered.map((stage) => [stage.id, stage]));

  return (
    <section className="panel palette" aria-label="Blok tersedia">
      <header className="panel__head">
        <h2 className="panel__title">Blok</h2>
      </header>

      {offered.length === 0 ? (
        <p className="panel__empty">Semua blok sudah ada di board.</p>
      ) : (
        <>
          <p className="palette__hint">
            Seret ke rack, atau tekan Enter untuk menaruhnya di ujung rantai.
          </p>

          {/* The list scrolls inside itself rather than growing the sidebar. An
              unbounded palette pushes the device panel below the fold, and how
              far depends on how many blocks happen to be unplaced -- so the
              panels underneath would move about for reasons nobody chose. */}
          <div className="palette__list">
            {GROUPS.map((group) => {
              const items = group.ids
                .map((id) => byId.get(id))
                .filter(Boolean) as EffectDescriptor[];
              if (items.length === 0) return null;

              return (
                <div key={group.title} className="palette__group">
                  <p className="palette__group-title">{group.title}</p>

                  {items.map((stage) => (
                    <button
                      key={stage.id}
                      type="button"
                      className={`palette__chip${
                        activeId === stage.id ? ' palette__chip--dragging' : ''
                      }`}
                      style={{ ['--hue' as string]: EFFECT_ACCENTS[stage.id] ?? '#4da3ff' }}
                      disabled={disabled}
                      data-palette-stage={stage.id}
                      title={
                        stage.id === 'split'
                          ? 'Buka jalur kedua - Mixer menyusul sendiri'
                          : stage.description
                      }
                      {...paletteProps(stage.id)}
                    >
                      <i aria-hidden="true" />
                      {stage.label}
                    </button>
                  ))}
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
