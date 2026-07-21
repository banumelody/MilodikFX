import { EFFECT_ACCENTS } from './EffectRack';
import type { EffectDescriptor } from '../services/api';

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
  // Input routing and the global controls are not stages you can see signal pass
  // through, so they stay out of the picture.
  const stages = effects.filter((effect) => effect.id !== 'input' && effect.id !== 'global');

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
