import { EFFECT_ACCENTS } from './EffectRack';
import type { EffectDescriptor } from '../services/api';

const NOT_A_STAGE = new Set(['input', 'global', 'metronome']);

export interface ChainStripProps {
  effects: EffectDescriptor[];
  onSelect: (effectId: string) => void;
  onToggle: (effectId: string, enabled: boolean) => void;
  disabled?: boolean;
}

/**
 * The signal path as a row of connected blocks, IN through to OUT.
 *
 * The rack below is a wrapping grid, which gives no hint that these stages run
 * in series. This strip is where the order is actually visible.
 */
export function ChainStrip({ effects, onSelect, onToggle, disabled = false }: ChainStripProps) {
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

        return (
          <div className="chain__item" key={effect.id}>
            <span className="chain__link" aria-hidden="true" />
            <button
              type="button"
              className={`chain__block${effect.enabled ? '' : ' chain__block--off'}`}
              style={{ '--accent': accent } as React.CSSProperties}
              disabled={disabled}
              title={
                canToggle
                  ? `${effect.label} — klik untuk menuju, klik kanan untuk hidup/mati`
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

export default ChainStrip;
