import { memo } from 'react';

import { EFFECT_ACCENTS } from './EffectRack';
import { effectType } from '../services/api';
import type { EffectDescriptor } from '../services/api';
import { useT } from '../i18n';

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
  /** Stages running on path B, so the router can draw them on their own line. */
  busB?: Set<string>;
}

/**
 * The signal path as a row of connected blocks, IN through to OUT.
 *
 * The rack below is a wrapping grid, which gives no hint that these stages run
 * in series. This strip is where the order is actually visible.
 *
 * Between a Splitter and a Mixer it becomes a **router**, drawn as two lines,
 * following Logic Pro's Pedalboard: the *upper* line is bus B and the lower one
 * is bus A. That is Apple's convention and it earns itself -- path A runs
 * straight through and B is the detour drawn over the top.
 */
function ChainStripBase({
  effects,
  onSelect,
  onToggle,
  disabled = false,
  dragHandleProps,
  draggingId = null,
  dropTargetId = null,
  busB,
}: ChainStripProps) {
  const t = useT();

  // Input routing, the global controls and the metronome are not stages the
  // guitar passes through, so they stay out of the picture. The metronome is
  // genuinely outside the chain -- it is mixed in after the master stage.
  // Anything off the board is not in the path at all.
  const stages = effects.filter(
    (effect) => !NOT_A_STAGE.has(effect.id) && effect.placed !== false,
  );

  const renderBlock = (effect: EffectDescriptor) => {
    const accent = EFFECT_ACCENTS[effectType(effect.id)] ?? '#4da3ff';
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
              ? t('chain.stageHint', { name: effect.label })
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
  };

  // Nothing loaded yet is not the same as nothing placed. Before the first
  // /api/effects lands there is no chain to describe, and drawing a wire then
  // would claim the board is empty when it is merely unknown.
  if (effects.length === 0) return null;

  if (stages.length === 0) {
    // An empty board *is* a straight wire, not a broken one. Fractal's grid
    // crosses empty spaces with passive shunts and this says the same thing.
    return (
      <nav className="chain chain--empty" aria-label="Rantai sinyal">
        <span className="chain__terminal">IN</span>
        <span className="chain__link" aria-hidden="true" />
        <span className="chain__empty">{t('board.straightWire')}</span>
        <span className="chain__link" aria-hidden="true" />
        <span className="chain__terminal">OUT</span>
      </nav>
    );
  }

  const splitAt = stages.findIndex((stage) => stage.id === 'split');
  const mixerAt = stages.findIndex((stage) => stage.id === 'mixer');
  const hasSection = splitAt >= 0 && mixerAt > splitAt;

  const pre = hasSection ? stages.slice(0, splitAt) : stages;
  const between = hasSection ? stages.slice(splitAt + 1, mixerAt) : [];
  const post = hasSection ? stages.slice(mixerAt + 1) : [];

  const onB = busB ?? new Set<string>();
  const rails: Array<{ bus: 'A' | 'B'; items: EffectDescriptor[] }> = [
    { bus: 'B', items: between.filter((stage) => onB.has(stage.id)) },
    { bus: 'A', items: between.filter((stage) => !onB.has(stage.id)) },
  ];

  return (
    <nav className="chain" aria-label="Rantai sinyal">
      <span className="chain__terminal">IN</span>

      {pre.map(renderBlock)}

      {hasSection ? (
        <>
          {renderBlock(stages[splitAt])}

          <div className="chain__fork">
            {rails.map(({ bus, items }) => (
              <div
                key={bus}
                className={`chain__rail chain__rail--${bus.toLowerCase()}`}
                aria-label={t('chain.busAria', { bus })}
              >
                <span className="chain__rail-tag" aria-hidden="true">
                  {bus}
                </span>
                {items.length === 0 ? (
                  <span className="chain__rail-bare">lewat begitu saja</span>
                ) : (
                  items.map(renderBlock)
                )}
              </div>
            ))}
          </div>

          {renderBlock(stages[mixerAt])}
          {post.map(renderBlock)}
        </>
      ) : null}

      <span className="chain__link" aria-hidden="true" />
      <span className="chain__terminal">OUT</span>
    </nav>
  );
}

/** Memoised: always on screen, re-rendered at 22 Hz, props stable per frame. */
export const ChainStrip = memo(ChainStripBase);

export default ChainStrip;
