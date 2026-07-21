import { Knob } from './Knob';
import { Toggle } from './Toggle';
import { ToneCurve } from './ToneCurve';
import type { EffectDescriptor, ParameterDescriptor } from '../services/api';

/** Accent per effect, so the rack reads as a signal chain rather than a list. */
export const EFFECT_ACCENTS: Record<string, string> = {
  global: '#8b95a7',
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
  metronome: '#8b95a7',
};

/**
 * Parameters whose numeric value is really a choice. The engine stores them as
 * numbers so everything stays uniform on the wire; only the labels live here.
 */
const ENUM_OPTIONS: Record<string, string[]> = {
  'input.mode': ['Mono - Input 1', 'Mono - Input 2', 'Mono - Sum both', 'Stereo'],
  'overdrive.oversampling': ['Mati', '2x', '4x', '8x'],
  // Order fixed by DelayProcessor::SyncDivision; the index is what the engine
  // stores, so these labels must stay lined up with it.
  'delay.syncMode': ['Mati', '1/4', '1/8.', '1/8', '1/8T', '1/16'],
};

/**
 * Parameters that another control overrides, and the value at which it does.
 * Locking a delay to the tempo makes its Time knob a lie, so the knob is
 * disabled rather than left showing a number the delay is not using.
 */
const OVERRIDDEN_BY: Record<string, { parameter: string; whenNot: number }> = {
  'delay.timeMs': { parameter: 'syncMode', whenNot: 0 },
};

export interface EffectRackProps {
  effect: EffectDescriptor;
  index?: number;
  total?: number;
  onParameterChange: (effectId: string, parameterId: string, value: number | string) => void;
  onEnabledChange: (effectId: string, enabled: boolean) => void;
  disabled?: boolean;
  /** Only affects where the tone curve puts Nyquist; harmless when unknown. */
  sampleRate?: number;
}

function formatValue(parameter: ParameterDescriptor, value: number) {
  if (parameter.unit === 'Hz' && value >= 1000) return `${(value / 1000).toFixed(1)}k`;
  if (parameter.step >= 1) return value.toFixed(0);
  if (parameter.step >= 0.1) return value.toFixed(1);
  return value.toFixed(2);
}

export function EffectRack({
  effect,
  index,
  total,
  onParameterChange,
  onEnabledChange,
  disabled = false,
  sampleRate,
}: EffectRackProps) {
  const accent = EFFECT_ACCENTS[effect.id] ?? '#4da3ff';
  const inactive = disabled || !effect.enabled;

  return (
    <section
      className={`rack${effect.enabled ? '' : ' rack--off'}`}
      aria-label={effect.label}
      id={`rack-${effect.id}`}
      style={{ '--accent': accent } as React.CSSProperties}
    >
      <header className="rack__head">
        <div className="rack__titles">
          <h2 className="rack__title">
            {index !== undefined && total !== undefined ? (
              <span className="rack__index">
                {index}/{total}
              </span>
            ) : null}
            {effect.label}
          </h2>
          <p className="rack__subtitle">{effect.description}</p>
        </div>
        {/* The input router and the master output are always in the path. A
            header switch there would look like a bypass but silence the app. */}
        {effect.toggleable === false ? null : (
          <Toggle
            checked={effect.enabled}
            accent={accent}
            label={`${effect.label} on/off`}
            disabled={disabled}
            onChange={(next) => onEnabledChange(effect.id, next)}
          />
        )}
      </header>

      <ToneCurve effect={effect} sampleRate={sampleRate} accent={accent} />

      <div className="rack__body">
        {effect.parameters.map((parameter) => {
          const enumKey = `${effect.id}.${parameter.id}`;
          const options = ENUM_OPTIONS[enumKey];

          // A text parameter (an impulse response, say) picks from whatever the
          // engine reports is on disk, so the list is never hardcoded here.
          if (parameter.type === 'text') {
            const choices = parameter.options ?? [];

            return (
              <label key={parameter.id} className="rack__select">
                <span className="rack__select-label">{parameter.label}</span>
                <select
                  value={String(parameter.value ?? '')}
                  disabled={inactive}
                  onChange={(event) =>
                    onParameterChange(effect.id, parameter.id, event.target.value)
                  }
                >
                  <option value="">
                    {choices.length === 0 ? 'Belum ada berkas' : 'Tidak dipakai'}
                  </option>
                  {choices.map((choice) => (
                    <option key={choice} value={choice}>
                      {choice}
                    </option>
                  ))}
                </select>
              </label>
            );
          }

          if (options) {
            return (
              <label key={parameter.id} className="rack__select">
                <span className="rack__select-label">{parameter.label}</span>
                <select
                  value={String(Math.round(Number(parameter.value)))}
                  disabled={inactive}
                  onChange={(event) =>
                    onParameterChange(effect.id, parameter.id, Number(event.target.value))
                  }
                >
                  {options.map((option, optionIndex) => (
                    <option key={option} value={optionIndex}>
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
                  checked={Number(parameter.value) >= 0.5}
                  accent={accent}
                  label={parameter.label}
                  disabled={inactive}
                  onChange={(next) => onParameterChange(effect.id, parameter.id, next ? 1 : 0)}
                />
                <span className="rack__switch-label">{parameter.label}</span>
              </div>
            );
          }

          const override = OVERRIDDEN_BY[enumKey];
          const overridden =
            override != null &&
            Math.round(
              Number(
                effect.parameters.find((other) => other.id === override.parameter)?.value ?? 0,
              ),
            ) !== override.whenNot;

          return (
            <Knob
              key={parameter.id}
              value={Number(parameter.value)}
              min={parameter.min}
              max={parameter.max}
              step={parameter.step}
              defaultValue={parameter.default}
              label={parameter.label}
              unit={parameter.unit}
              accent={accent}
              disabled={inactive || overridden}
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
