import { Fragment, memo } from 'react';

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
  nam: '#ff7b54',
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
  // Blend = one cabinet with two mics. Stereo = A left, B right, which is how a
  // real stereo rig is built: one mono amp into two cabinets panned apart.
  'cabinet.irMode': ['Blend A+B', 'Stereo (A kiri / B kanan)'],
  // Order fixed by drive::Type in DriveVoicing.h; the index is what presets
  // store, so these may be appended to but never reordered.
  'overdrive.type': [
    'Custom',
    'Tube Screamer',
    'Bluesbreaker',
    'Blues Driver',
    'Transparent',
    'OCD',
    'Dumble',
    'Marshall-in-a-Box',
    'Clean Boost',
    'Centaur',
    'RAT',
    'Big Muff',
  ],
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

/**
 * A sensible oversampling default per drive voicing, indexed by drive::Type.
 * Written only when the user picks a voicing from the Tipe dropdown, never on a
 * preset restore (which sets parameters straight through the registry) -- so the
 * user's own oversampling choice is always respected until they change type
 * again. Fuzz and hard clip make the most high harmonics and want the most
 * headroom; a clean boost barely clips and needs none. 0=Mati 1=2x 2=4x 3=8x.
 */
const RECOMMENDED_OVERSAMPLING: Record<number, number> = {
  0: 1, // Custom
  1: 1, // Tube Screamer
  2: 1, // Bluesbreaker
  3: 1, // Blues Driver
  4: 1, // Transparent
  5: 2, // OCD
  6: 1, // Dumble
  7: 2, // Marshall-in-a-Box
  8: 0, // Clean Boost
  9: 1, // Centaur
  10: 2, // RAT
  11: 2, // Big Muff
};

/**
 * Which controls each drive voicing has, and what the original pedal called
 * them.
 *
 * The engine registers the union of every voicing's parameters once, so the
 * registry stays a flat stable set that presets and settings can rely on.
 * Which of them a given voicing actually uses is a presentation question, and
 * this is where it is answered. A Tube Screamer has no Bass knob and a
 * Bluesbreaker's gain control says Gain, not Drive -- showing every parameter
 * for every type would be showing controls that do nothing.
 */
const DRIVE_CONTROLS: Record<
  number,
  { show: string[]; labels?: Record<string, string> }
> = {
  0: { show: ['drivePct', 'levelPct', 'asymmetry', 'oversampling'] },
  1: { show: ['drivePct', 'tonePct', 'levelPct', 'oversampling'] },
  2: {
    show: ['drivePct', 'tonePct', 'levelPct', 'oversampling'],
    labels: { drivePct: 'Gain', levelPct: 'Volume' },
  },
  3: {
    show: ['drivePct', 'tonePct', 'levelPct', 'oversampling'],
    labels: { drivePct: 'Gain' },
  },
  4: {
    show: ['drivePct', 'bassDb', 'trebleDb', 'levelPct', 'oversampling'],
    labels: { drivePct: 'Gain' },
  },
  5: { show: ['drivePct', 'tonePct', 'levelPct', 'hpMode', 'oversampling'] },
  6: {
    show: ['drivePct', 'voicePct', 'tonePct', 'levelPct', 'oversampling'],
    labels: { drivePct: 'Gain' },
  },
  7: {
    show: ['drivePct', 'bassDb', 'midDb', 'trebleDb', 'levelPct', 'oversampling'],
    labels: { drivePct: 'Gain' },
  },
  8: {
    show: ['bright', 'drivePct', 'levelPct', 'oversampling'],
    labels: { drivePct: 'Boost' },
  },
  // Centaur: Gain, Treble (the tone sweep), Output -- the three the pedal has.
  9: {
    show: ['drivePct', 'tonePct', 'levelPct', 'oversampling'],
    labels: { drivePct: 'Gain', tonePct: 'Treble', levelPct: 'Output' },
  },
  // RAT: Distortion, Filter (runs backwards, clockwise is darker), Volume.
  10: {
    show: ['drivePct', 'tonePct', 'levelPct', 'oversampling'],
    labels: { drivePct: 'Distortion', tonePct: 'Filter', levelPct: 'Volume' },
  },
  // Big Muff: Sustain, Tone (the scooped-mid stack), Volume.
  11: {
    show: ['drivePct', 'tonePct', 'levelPct', 'oversampling'],
    labels: { drivePct: 'Sustain', tonePct: 'Tone', levelPct: 'Volume' },
  },
};

export interface EffectRackProps {
  effect: EffectDescriptor;
  index?: number;
  total?: number;
  onParameterChange: (effectId: string, parameterId: string, value: number | string) => void;
  onEnabledChange: (effectId: string, enabled: boolean) => void;
  /** Selects a channel (A/B/C/D). Absent when the engine does not track them. */
  onChannelSelect?: (effectId: string, index: number) => void;
  /** "<effect>.<parameter>" keys a modifier currently owns; those knobs go inert. */
  modulatedParams?: Set<string>;
  /** "<effect>.<parameter>" keys pinned to the Perform screen by this preset. */
  pinnedParams?: Set<string>;
  /** Pins or unpins a control. Absent when the engine does not track pins. */
  onTogglePin?: (effectId: string, parameterId: string) => void;
  /**
   * Moves this stage earlier (-1) or later (+1) in the signal chain. Absent when
   * the engine does not support reordering.
   */
  onMove?: (effectId: string, delta: number) => void;
  /** False for a stage the engine pins in place; the reason is shown as a hint. */
  movable?: boolean;
  canMoveUp?: boolean;
  canMoveDown?: boolean;
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

function EffectRackBase({
  effect,
  index,
  total,
  onParameterChange,
  onEnabledChange,
  onChannelSelect,
  modulatedParams,
  pinnedParams,
  onTogglePin,
  onMove,
  movable = true,
  canMoveUp = false,
  canMoveDown = false,
  disabled = false,
  sampleRate,
}: EffectRackProps) {
  const accent = EFFECT_ACCENTS[effect.id] ?? '#4da3ff';
  const inactive = disabled || !effect.enabled;

  // Channel tabs (A/B/C/D) belong on the real effect blocks -- the ones with a
  // bypass -- not on the input router or the master out.
  const showChannels =
    effect.toggleable !== false &&
    Array.isArray(effect.channels) &&
    effect.channels.length > 0 &&
    onChannelSelect !== undefined;
  const activeChannel = Math.round(Number(effect.channel ?? 0));

  // The overdrive registers every voicing's controls; only the selected
  // voicing's are worth showing, under the names its original used.
  const visibleParameters = (() => {
    if (effect.id !== 'overdrive') return effect.parameters;

    const type = Math.round(
      Number(effect.parameters.find((p) => p.id === 'type')?.value ?? 0),
    );
    const layout = DRIVE_CONTROLS[type] ?? DRIVE_CONTROLS[0];

    const ordered: ParameterDescriptor[] = [];

    const typeParameter = effect.parameters.find((p) => p.id === 'type');
    if (typeParameter) ordered.push(typeParameter);

    for (const id of layout.show) {
      const found = effect.parameters.find((p) => p.id === id);
      if (!found) continue;

      const label = layout.labels?.[id];
      ordered.push(label ? { ...found, label } : found);
    }

    return ordered;
  })();

  return (
    <section
      className={`rack${effect.enabled ? '' : ' rack--off'}`}
      aria-label={effect.label}
      id={`rack-${effect.id}`}
      style={{ '--accent': accent } as React.CSSProperties}
    >
      <header className="rack__head">
        {onMove ? (
          <div className="rack__move" role="group" aria-label={`Posisi ${effect.label}`}>
            {movable ? (
              <>
                <button
                  type="button"
                  className="rack__move-btn"
                  disabled={disabled || !canMoveUp}
                  aria-label={`Pindahkan ${effect.label} lebih awal di rantai`}
                  title="Lebih awal di rantai"
                  onClick={() => onMove(effect.id, -1)}
                >
                  ▲
                </button>
                <button
                  type="button"
                  className="rack__move-btn"
                  disabled={disabled || !canMoveDown}
                  aria-label={`Pindahkan ${effect.label} lebih akhir di rantai`}
                  title="Lebih akhir di rantai"
                  onClick={() => onMove(effect.id, 1)}
                >
                  ▼
                </button>
              </>
            ) : (
              // Not merely disabled: explained. The input trim has to stay first
              // for the input meter's arithmetic to be true, and the master has
              // to stay last because it carries the safety limiter.
              <span
                className="rack__move-locked"
                title={
                  effect.id === 'master'
                    ? 'Selalu terakhir — membawa limiter pengaman'
                    : 'Selalu pertama — meter input mengandalkan posisinya'
                }
                aria-label="Posisi terkunci"
              >
                <svg viewBox="0 0 16 16" aria-hidden="true" focusable="false">
                  <rect x="3.5" y="7" width="9" height="6.5" rx="1.2" fill="currentColor" />
                  <path
                    d="M5.5 7V5a2.5 2.5 0 015 0v2"
                    stroke="currentColor"
                    strokeWidth="1.4"
                    fill="none"
                  />
                </svg>
              </span>
            )}
          </div>
        ) : null}
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

      {showChannels ? (
        <div className="rack__channels" role="tablist" aria-label={`${effect.label} channel`}>
          {effect.channels!.map((name, i) => (
            <button
              key={i}
              type="button"
              role="tab"
              aria-selected={i === activeChannel}
              className={`rack__channel${i === activeChannel ? ' rack__channel--active' : ''}`}
              style={i === activeChannel ? { borderColor: accent, color: accent } : undefined}
              // Switchable even while the effect is bypassed, so a channel can be
              // dialled in before it is needed; only a dead engine disables it.
              disabled={disabled}
              title={`Channel ${name}`}
              onClick={() => onChannelSelect?.(effect.id, i)}
            >
              {name}
            </button>
          ))}
        </div>
      ) : null}

      <ToneCurve effect={effect} sampleRate={sampleRate} accent={accent} />

      <div className="rack__body">
        {visibleParameters.map((parameter) => {
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
                  onChange={(event) => {
                    const next = Number(event.target.value);
                    onParameterChange(effect.id, parameter.id, next);

                    // Picking a drive voicing also nudges oversampling to a
                    // value that suits it -- fuzz gets headroom, a clean boost
                    // does not pay for what it will not use. Only here, on the
                    // explicit dropdown choice; a manual oversampling turn after
                    // this stays put, and a preset restore never comes through
                    // this path at all.
                    if (enumKey === 'overdrive.type') {
                      const recommended = RECOMMENDED_OVERSAMPLING[next];
                      if (recommended !== undefined)
                        onParameterChange(effect.id, 'oversampling', recommended);
                    }
                  }}
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

          // A modifier owns this parameter, but the knob stays live: it sets the
          // centre the sweep rides on (base + offset). The value the engine
          // reports for a modulated parameter is that centre, so the knob does
          // not chase the sweep; a MOD tag marks that it is being modulated.
          const modulated = modulatedParams?.has(`${effect.id}.${parameter.id}`) ?? false;

          const knob = (
            <Knob
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

          const pinned = pinnedParams?.has(`${effect.id}.${parameter.id}`) ?? false;

          // A bare knob stays bare. It only gets a wrapper when there is
          // something to overlay on it — the MOD tag, the pin button, or both.
          if (!modulated && !onTogglePin) return <Fragment key={parameter.id}>{knob}</Fragment>;

          return (
            <div
              key={parameter.id}
              className={`rack__modknob${modulated ? ' rack__modknob--live' : ''}`}
              title={
                modulated ? 'Modifier aktif — knob menyetel titik tengah sapuan' : undefined
              }
            >
              {knob}
              {modulated && <span className="rack__modtag">MOD</span>}
              {onTogglePin && (
                <button
                  type="button"
                  className={`rack__pin${pinned ? ' rack__pin--on' : ''}`}
                  aria-pressed={pinned}
                  aria-label={
                    pinned
                      ? `Lepas ${parameter.label} dari layar Perform`
                      : `Sematkan ${parameter.label} ke layar Perform`
                  }
                  title={
                    pinned
                      ? 'Tersemat di Perform — klik untuk melepas'
                      : 'Sematkan ke layar Perform'
                  }
                  onClick={() => onTogglePin(effect.id, parameter.id)}
                >
                  <svg viewBox="0 0 16 16" aria-hidden="true" focusable="false">
                    <path
                      d="M9.5 1.5 14 6l-2 .6-3 3L8.4 13 3 7.6l3.4-.6 3-3z"
                      fill="currentColor"
                    />
                    <path d="M6 10 2 14" stroke="currentColor" strokeWidth="1.4" fill="none" />
                  </svg>
                </button>
              )}
            </div>
          );
        })}
      </div>
    </section>
  );
}

/**
 * Memoised: this is the heavy child. A dozen of these, each with its dials and
 * tone curve, must not re-render on every meter frame. Props are stable across
 * a frame -- the effect object only changes when that effect changes, the
 * callbacks are stabilised in App, and sampleRate is a steady number -- so the
 * memo skips all of them until something the card actually shows moves.
 */
export const EffectRack = memo(EffectRackBase);

export default EffectRack;
