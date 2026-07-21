import { Knob } from './Knob';
import { Toggle } from './Toggle';
import type { EffectDescriptor, ParameterDescriptor } from '../services/api';

/** Accent per effect, so the rack reads as a signal chain rather than a list. */
export const EFFECT_ACCENTS: Record<string, string> = {
  input: '#8b95a7',
  noiseGate: '#5ac8fa',
  cleanBoost: '#4da3ff',
  compressor: '#7c6cff',
  overdrive: '#ff8a3d',
  eq: '#28c76f',
  toneStack: '#00c2a8',
  cabinet: '#d98cff',
  delay: '#ffc93c',
  reverb: '#4dd0ff',
  master: '#ff5c7a',
};

/**
 * Parameters whose numeric value is really a choice. The engine stores them as
 * numbers so everything stays uniform on the wire; only the labels live here.
 */
const ENUM_OPTIONS: Record<string, string[]> = {
  'input.mode': ['Mono - Input 1', 'Mono - Input 2', 'Mono - Sum both', 'Stereo'],
};

export interface EffectRackProps {
  effect: EffectDescriptor;
  onParameterChange: (effectId: string, parameterId: string, value: number) => void;
  onEnabledChange: (effectId: string, enabled: boolean) => void;
  disabled?: boolean;
}

function formatValue(parameter: ParameterDescriptor, value: number) {
  if (parameter.unit === 'Hz' && value >= 1000) return `${(value / 1000).toFixed(1)}k`;
  if (parameter.step >= 1) return value.toFixed(0);
  if (parameter.step >= 0.1) return value.toFixed(1);
  return value.toFixed(2);
}

export function EffectRack({
  effect,
  onParameterChange,
  onEnabledChange,
  disabled = false,
}: EffectRackProps) {
  const accent = EFFECT_ACCENTS[effect.id] ?? '#4da3ff';
  const inactive = disabled || !effect.enabled;

  return (
    <section className={`rack${effect.enabled ? '' : ' rack--off'}`} aria-label={effect.label}>
      <header className="rack__head">
        <span className="rack__dot" style={{ backgroundColor: accent }} aria-hidden="true" />
        <div className="rack__titles">
          <h2 className="rack__title">{effect.label}</h2>
          <p className="rack__subtitle">{effect.description}</p>
        </div>
        {effect.id === 'input' ? null : (
          <Toggle
            checked={effect.enabled}
            accent={accent}
            label={`${effect.label} on/off`}
            disabled={disabled}
            onChange={(next) => onEnabledChange(effect.id, next)}
          />
        )}
      </header>

      <div className="rack__body">
        {effect.parameters.map((parameter) => {
          const enumKey = `${effect.id}.${parameter.id}`;
          const options = ENUM_OPTIONS[enumKey];

          if (options) {
            return (
              <label key={parameter.id} className="rack__select">
                <span className="rack__select-label">{parameter.label}</span>
                <select
                  value={String(Math.round(parameter.value))}
                  disabled={inactive}
                  onChange={(event) =>
                    onParameterChange(effect.id, parameter.id, Number(event.target.value))
                  }
                >
                  {options.map((option, index) => (
                    <option key={option} value={index}>
                      {option}
                    </option>
                  ))}
                </select>
              </label>
            );
          }

          if (parameter.type === 'bool') {
            return (
              <div key={parameter.id} className="rack__switch">
                <Toggle
                  checked={parameter.value >= 0.5}
                  accent={accent}
                  label={parameter.label}
                  disabled={inactive}
                  onChange={(next) => onParameterChange(effect.id, parameter.id, next ? 1 : 0)}
                />
                <span className="rack__switch-label">{parameter.label}</span>
              </div>
            );
          }

          return (
            <Knob
              key={parameter.id}
              value={parameter.value}
              min={parameter.min}
              max={parameter.max}
              step={parameter.step}
              defaultValue={parameter.default}
              label={parameter.label}
              unit={parameter.unit}
              accent={accent}
              disabled={inactive}
              format={(value) => formatValue(parameter, value)}
              onChange={(value) => onParameterChange(effect.id, parameter.id, value)}
            />
          );
        })}
      </div>
    </section>
  );
}

export default EffectRack;
