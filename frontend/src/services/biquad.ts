/**
 * Frequency response of the EQ and Contour stages, for drawing the curve.
 *
 * A direct port of `src/dsp/Biquad.h` -- the same RBJ formulas, the same band
 * frequencies and Qs read out of EQProcessor.cpp and ToneStackProcessor.cpp. It
 * is a port rather than something the engine sends because the parameters are
 * already on the wire in /api/effects; adding a curve endpoint would mean a new
 * realtime data path for something the browser can work out itself.
 *
 * If a band's frequency or Q changes in the C++, it has to change here too.
 * There is nothing that enforces that, which is why the numbers are named and
 * kept next to each other rather than scattered through the drawing code.
 */

export interface BiquadCoeffs {
  b0: number;
  b1: number;
  b2: number;
  a1: number;
  a2: number;
}

const IDENTITY: BiquadCoeffs = { b0: 1, b1: 0, b2: 0, a1: 0, a2: 0 };

/** Keeps w0 strictly inside (0, pi) so the RBJ formulas stay well defined. */
function safeOmega(freqHz: number, sampleRate: number): number {
  const nyquistLimit = Math.max(1, sampleRate * 0.45);
  const f = Math.min(nyquistLimit, Math.max(1, freqHz));
  return (2 * Math.PI * f) / Math.max(1, sampleRate);
}

function normalise(
  b0: number,
  b1: number,
  b2: number,
  a0: number,
  a1: number,
  a2: number,
): BiquadCoeffs {
  if (!Number.isFinite(a0) || Math.abs(a0) < 1e-12) return IDENTITY;

  const coeffs = { b0: b0 / a0, b1: b1 / a0, b2: b2 / a0, a1: a1 / a0, a2: a2 / a0 };

  return Object.values(coeffs).every(Number.isFinite) ? coeffs : IDENTITY;
}

export function makePeak(
  sampleRate: number,
  freqHz: number,
  q: number,
  gainDb: number,
): BiquadCoeffs {
  const A = Math.pow(10, gainDb / 40);
  const w0 = safeOmega(freqHz, sampleRate);
  const cosW0 = Math.cos(w0);
  const alpha = Math.sin(w0) / (2 * Math.max(0.05, q));

  return normalise(
    1 + alpha * A,
    -2 * cosW0,
    1 - alpha * A,
    1 + alpha / A,
    -2 * cosW0,
    1 - alpha / A,
  );
}

export function makeLowShelf(sampleRate: number, freqHz: number, gainDb: number): BiquadCoeffs {
  const A = Math.pow(10, gainDb / 40);
  const w0 = safeOmega(freqHz, sampleRate);
  const cosW0 = Math.cos(w0);
  const alpha = Math.sin(w0) * 0.5 * Math.SQRT2;
  const twoSqrtAAlpha = 2 * Math.sqrt(A) * alpha;

  return normalise(
    A * (A + 1 - (A - 1) * cosW0 + twoSqrtAAlpha),
    2 * A * (A - 1 - (A + 1) * cosW0),
    A * (A + 1 - (A - 1) * cosW0 - twoSqrtAAlpha),
    A + 1 + (A - 1) * cosW0 + twoSqrtAAlpha,
    -2 * (A - 1 + (A + 1) * cosW0),
    A + 1 + (A - 1) * cosW0 - twoSqrtAAlpha,
  );
}

export function makeHighShelf(sampleRate: number, freqHz: number, gainDb: number): BiquadCoeffs {
  const A = Math.pow(10, gainDb / 40);
  const w0 = safeOmega(freqHz, sampleRate);
  const cosW0 = Math.cos(w0);
  const alpha = Math.sin(w0) * 0.5 * Math.SQRT2;
  const twoSqrtAAlpha = 2 * Math.sqrt(A) * alpha;

  return normalise(
    A * (A + 1 + (A - 1) * cosW0 + twoSqrtAAlpha),
    -2 * A * (A - 1 + (A + 1) * cosW0),
    A * (A + 1 + (A - 1) * cosW0 - twoSqrtAAlpha),
    A + 1 - (A - 1) * cosW0 + twoSqrtAAlpha,
    2 * (A - 1 - (A + 1) * cosW0),
    A + 1 - (A - 1) * cosW0 - twoSqrtAAlpha,
  );
}

/** Magnitude of a biquad at one frequency, in dB. */
export function magnitudeDb(coeffs: BiquadCoeffs, freqHz: number, sampleRate: number): number {
  const w = (2 * Math.PI * freqHz) / Math.max(1, sampleRate);
  const cosW = Math.cos(w);
  const sinW = Math.sin(w);
  const cos2W = Math.cos(2 * w);
  const sin2W = Math.sin(2 * w);

  // H(e^jw) evaluated directly rather than via the squared-magnitude identity,
  // which is easier to get subtly wrong when the coefficients are not symmetric.
  const numeratorReal = coeffs.b0 + coeffs.b1 * cosW + coeffs.b2 * cos2W;
  const numeratorImag = -(coeffs.b1 * sinW + coeffs.b2 * sin2W);
  const denominatorReal = 1 + coeffs.a1 * cosW + coeffs.a2 * cos2W;
  const denominatorImag = -(coeffs.a1 * sinW + coeffs.a2 * sin2W);

  const numerator = Math.hypot(numeratorReal, numeratorImag);
  const denominator = Math.hypot(denominatorReal, denominatorImag);

  if (denominator < 1e-12) return 0;

  const magnitude = numerator / denominator;

  return magnitude > 1e-9 ? 20 * Math.log10(magnitude) : -180;
}

interface Band {
  parameter: string;
  freqHz: number;
  q: number;
  kind: 'peak' | 'lowShelf' | 'highShelf';
}

/**
 * The bands each tone stage actually builds. Mirrors the constants at the top
 * of EQProcessor.cpp and ToneStackProcessor.cpp.
 */
export const TONE_BANDS: Record<string, Band[]> = {
  eq: [
    { parameter: 'bassDb', freqHz: 120, q: Math.SQRT1_2, kind: 'lowShelf' },
    { parameter: 'midDb', freqHz: 1000, q: 0.9, kind: 'peak' },
    { parameter: 'trebleDb', freqHz: 7000, q: Math.SQRT1_2, kind: 'highShelf' },
  ],
  toneStack: [
    { parameter: 'bassDb', freqHz: 50, q: 0.7, kind: 'peak' },
    { parameter: 'midDb', freqHz: 500, q: 0.7, kind: 'peak' },
    { parameter: 'trebleDb', freqHz: 5000, q: 0.7, kind: 'peak' },
  ],
};

function bandCoeffs(band: Band, gainDb: number, sampleRate: number): BiquadCoeffs {
  if (band.kind === 'lowShelf') return makeLowShelf(sampleRate, band.freqHz, gainDb);
  if (band.kind === 'highShelf') return makeHighShelf(sampleRate, band.freqHz, gainDb);
  return makePeak(sampleRate, band.freqHz, band.q, gainDb);
}

export interface CurvePoint {
  freqHz: number;
  db: number;
}

/**
 * The combined response of a tone stage, sampled logarithmically.
 *
 * Log spacing because that is how the ear hears it and how the plot is drawn --
 * linear spacing would put almost every point above 10 kHz.
 */
export function computeToneCurve(
  effectId: string,
  gains: Record<string, number>,
  options: { sampleRate?: number; points?: number; minHz?: number; maxHz?: number } = {},
): CurvePoint[] {
  const bands = TONE_BANDS[effectId];

  if (!bands) return [];

  const sampleRate = options.sampleRate && options.sampleRate > 0 ? options.sampleRate : 48000;
  const points = options.points ?? 96;
  const minHz = options.minHz ?? 20;
  const maxHz = Math.min(options.maxHz ?? 20000, sampleRate * 0.45);

  const coeffs = bands.map((band) => bandCoeffs(band, gains[band.parameter] ?? 0, sampleRate));

  const curve: CurvePoint[] = [];
  const ratio = Math.log(maxHz / minHz);

  for (let i = 0; i < points; i += 1) {
    const freqHz = minHz * Math.exp((ratio * i) / (points - 1));

    // Cascaded, so the dB contributions add.
    const db = coeffs.reduce((sum, c) => sum + magnitudeDb(c, freqHz, sampleRate), 0);

    curve.push({ freqHz, db });
  }

  return curve;
}
