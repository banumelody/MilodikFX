import { memo } from 'react';

import { EFFECT_ACCENTS } from './EffectRack';
import type { EffectDescriptor } from '../services/api';

const NOT_A_STAGE = new Set(['input', 'global', 'metronome']);

export interface ChainStripProps {
  effects: EffectDescriptor[];
  onSelect: (effectId: string) => void;
  onToggle: (effectId: string, enabled: boolean) => void;
  disabled?: boolean;
  /**
   * Drag handlers per stage, from useChainReorder.
   *
   * The strip is the better place to reorder: dragging a small chip across a row
   * beats dragging a card the height of the screen past four others. It is also
   * where the order is actually legible, so a change here reads immediately.
   */
  dragHandleProps?: (effectId: string) => Record<string, unknown>;
  draggingId?: string | null;
  dropTargetId?: string | null;
}

/**
 * The signal path as a row of connected blocks, IN through to OUT.
 *
 * The rack below is a wrapping grid, which gives no hint that these stages run
 * in series. This strip is where the order is actually visible.
 */
function ChainStripBase({
  effects,
  onSelect,
  onToggle,
  disabled = false,
  dragHandleProps,
  draggingId = null,
  dropTargetId = null,
}: ChainStripProps) {
  // Input routing, the global controls and the metronome are not stages the
  // guitar passes through, so they stay out of the picture. The metronome is
  // genuinely outside the chain -- it is mixed in after the master stage.
  const stages = effects.filter((effect) => !NOT_A_STAGE.has(effect.id));

  if (stages.length === 0) return null;

  return (
    <nav className="chain" aria-label="Rantai sinyal">
      <span className="chain__terminal">IN</span>

      {stages.map((effect) => {
        const accent = EFFECT_ACCENTS[effect.id] ?? '#4da3ff';
        const canToggle = effect.toggleable !== false;

        const dragProps = dragHandleProps?.(effect.id);

        return (
          <div className="chain__item" key={effect.id}>
            <span className="chain__link" aria-hidden="true" />
            <button
              type="button"
              className={
                `chain__block${effect.enabled ? '' : ' chain__block--off'}` +
                `${draggingId === effect.id ? ' chain__block--dragging' : ''}` +
                `${
                  dropTargetId === effect.id && draggingId !== effect.id
                    ? ' chain__block--drop'
                    : ''
                }`
              }
              style={{ '--accent': accent } as React.CSSProperties}
              disabled={disabled}
              // Measured by useChainReorder when a drag starts, so the strip is a
              // drop target as well as a drag source.
              data-chain-stage={dragProps ? effect.id : undefined}
              {...(dragProps ?? {})}
              title={
                canToggle
                  ? `${effect.label} — klik untuk menuju, seret untuk menata ulang, klik kanan untuk hidup/mati`
                  : effect.label
              }
              aria-label={effect.label}
              aria-pressed={effect.enabled}
              onClick={() => onSelect(effect.id)}
              onContextMenu={(event) => {
                if (!canToggle) return;
                // Right-click toggles in place: reaching for the card's switch
                // during a take is exactly the thing this row is meant to avoid.
                event.preventDefault();
                onToggle(effect.id, !effect.enabled);
              }}
            >
              <span className="chain__dot" aria-hidden="true" />
              <span className="chain__name">{effect.label}</span>
            </button>
          </div>
        );
      })}

      <span className="chain__link" aria-hidden="true" />
      <span className="chain__terminal">OUT</span>
    </nav>
  );
}

/** Memoised: always on screen, re-rendered at 22 Hz, props stable per frame. */
export const ChainStrip = memo(ChainStripBase);

export default ChainStrip;
