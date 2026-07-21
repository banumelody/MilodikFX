import { useMemo } from 'react';

import { TONE_BANDS, computeToneCurve } from '../services/biquad';
import type { EffectDescriptor } from '../services/api';

/** Vertical extent of the plot. Matches the +/-12 dB the knobs allow. */
const RANGE_DB = 14;

const VIEW_WIDTH = 300;
const VIEW_HEIGHT = 90;

const GRID_HZ = [100, 1000, 10000];

interface ToneCurveProps {
  effect: EffectDescriptor;
  sampleRate?: number;
  accent?: string;
}

function label(hz: number) {
  return hz >= 1000 ? `${hz / 1000}k` : String(hz);
}

/**
 * The frequency response of a tone stage, drawn over its knobs.
 *
 * Three numbers do not tell you what the stage is doing to the sound; the
 * shape does. Computed in the browser from parameters that are already on the
 * wire -- see services/biquad.ts for why it is a port rather than an endpoint.
 */
export function ToneCurve({ effect, sampleRate = 48000, accent = '#4da3ff' }: ToneCurveProps) {
  const bands = TONE_BANDS[effect.id];

  const gains = useMemo(() => {
    const result: Record<string, number> = {};

    for (const parameter of effect.parameters) result[parameter.id] = Number(parameter.value);

    return result;
  }, [effect.parameters]);

  const path = useMemo(() => {
    if (!bands) return '';

    const curve = computeToneCurve(effect.id, gains, { sampleRate });

    if (curve.length === 0) return '';

    const minHz = curve[0].freqHz;
    const maxHz = curve[curve.length - 1].freqHz;
    const span = Math.log(maxHz / minHz);

    return curve
      .map((point, index) => {
        const x = (Math.log(point.freqHz / minHz) / span) * VIEW_WIDTH;
        const clamped = Math.max(-RANGE_DB, Math.min(RANGE_DB, point.db));
        const y = VIEW_HEIGHT / 2 - (clamped / RANGE_DB) * (VIEW_HEIGHT / 2);

        return `${index === 0 ? 'M' : 'L'}${x.toFixed(1)},${y.toFixed(1)}`;
      })
      .join(' ');
  }, [bands, effect.id, gains, sampleRate]);

  if (!bands || !path) return null;

  const flat = effect.parameters.every(
    (parameter) => !(parameter.id in gains) || Math.abs(gains[parameter.id]) < 0.05,
  );

  return (
    <svg
      className={`tone-curve${effect.enabled ? '' : ' tone-curve--off'}`}
      viewBox={`0 0 ${VIEW_WIDTH} ${VIEW_HEIGHT}`}
      preserveAspectRatio="none"
      role="img"
      aria-label={`Kurva respons ${effect.label}${flat ? ', rata' : ''}`}
    >
      <line
        className="tone-curve__zero"
        x1="0"
        y1={VIEW_HEIGHT / 2}
        x2={VIEW_WIDTH}
        y2={VIEW_HEIGHT / 2}
      />

      {GRID_HZ.map((hz) => {
        const x = (Math.log(hz / 20) / Math.log(20000 / 20)) * VIEW_WIDTH;

        return (
          <g key={hz}>
            <line className="tone-curve__grid" x1={x} y1="0" x2={x} y2={VIEW_HEIGHT} />
            <text className="tone-curve__tick" x={x + 3} y={VIEW_HEIGHT - 4}>
              {label(hz)}
            </text>
          </g>
        );
      })}

      <path className="tone-curve__line" d={path} style={{ stroke: accent }} />
    </svg>
  );
}

export default ToneCurve;
