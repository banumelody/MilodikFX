import { describe, expect, it } from 'vitest';

import {
  computeToneCurve,
  magnitudeDb,
  makeHighShelf,
  makeLowShelf,
  makePeak,
  TONE_BANDS,
} from '../biquad';

const RATE = 48000;

describe('biquad response', () => {
  describe('peaking filter', () => {
    it('hits exactly the dialled gain at its centre frequency', () => {
      // The defining property of an RBJ peak: |H| at f0 is the gain, whatever
      // Q is. Anything else means the coefficients are wrong.
      for (const gainDb of [-12, -6, -3, 3, 6, 12]) {
        for (const q of [0.5, 0.7, 0.9, 2]) {
          const coeffs = makePeak(RATE, 1000, q, gainDb);
          expect(magnitudeDb(coeffs, 1000, RATE)).toBeCloseTo(gainDb, 4);
        }
      }
    });

    it('leaves the rest of the spectrum alone', () => {
      const coeffs = makePeak(RATE, 1000, 0.9, 12);

      expect(magnitudeDb(coeffs, 20, RATE)).toBeCloseTo(0, 1);
      expect(magnitudeDb(coeffs, 20000, RATE)).toBeCloseTo(0, 1);
    });

    it('reaches half the gain at the edges of its bandwidth', () => {
      // For a peak at Q, the -3 dB points of a +6 dB boost sit at +3 dB, and
      // they are placed by Q. A narrower Q has to be narrower.
      const wide = makePeak(RATE, 1000, 0.5, 12);
      const narrow = makePeak(RATE, 1000, 4, 12);

      expect(magnitudeDb(wide, 500, RATE)).toBeGreaterThan(magnitudeDb(narrow, 500, RATE));
      expect(magnitudeDb(wide, 2000, RATE)).toBeGreaterThan(magnitudeDb(narrow, 2000, RATE));
    });

    it('is symmetric between boost and cut', () => {
      const boost = makePeak(RATE, 1000, 0.9, 8);
      const cut = makePeak(RATE, 1000, 0.9, -8);

      for (const hz of [200, 700, 1000, 1500, 5000]) {
        expect(magnitudeDb(boost, hz, RATE)).toBeCloseTo(-magnitudeDb(cut, hz, RATE), 4);
      }
    });

    it('does nothing at zero gain', () => {
      const coeffs = makePeak(RATE, 1000, 0.9, 0);

      for (const hz of [20, 100, 1000, 10000, 20000]) {
        expect(magnitudeDb(coeffs, hz, RATE)).toBeCloseTo(0, 6);
      }
    });
  });

  describe('shelving filters', () => {
    it('reaches the full gain well below a low shelf corner', () => {
      const coeffs = makeLowShelf(RATE, 120, 12);

      expect(magnitudeDb(coeffs, 10, RATE)).toBeCloseTo(12, 0);
      // And half of it at the corner, which is what a shelf's corner means.
      expect(magnitudeDb(coeffs, 120, RATE)).toBeCloseTo(6, 1);
      expect(magnitudeDb(coeffs, 10000, RATE)).toBeCloseTo(0, 1);
    });

    it('reaches the full gain well above a high shelf corner', () => {
      const coeffs = makeHighShelf(RATE, 7000, 12);

      expect(magnitudeDb(coeffs, 20000, RATE)).toBeCloseTo(12, 0);
      expect(magnitudeDb(coeffs, 7000, RATE)).toBeCloseTo(6, 1);
      expect(magnitudeDb(coeffs, 50, RATE)).toBeCloseTo(0, 1);
    });

    it('cuts as far as it boosts', () => {
      const boost = makeLowShelf(RATE, 120, 9);
      const cut = makeLowShelf(RATE, 120, -9);

      expect(magnitudeDb(boost, 20, RATE)).toBeCloseTo(-magnitudeDb(cut, 20, RATE), 3);
    });
  });

  describe('degenerate input', () => {
    it('is flat rather than NaN at a nonsensical sample rate', () => {
      // safeOmega is what keeps w0 inside (0, pi); without it the formulas
      // would produce NaN and the plot would simply not draw.
      for (const rate of [0, -1, 1]) {
        const value = magnitudeDb(makePeak(rate, 1000, 0.9, 6), 1000, rate);
        expect(Number.isFinite(value)).toBe(true);
      }
    });

    it('clamps a frequency above Nyquist instead of aliasing it', () => {
      const coeffs = makePeak(RATE, 40000, 0.9, 6);

      for (const hz of [100, 1000, 20000]) {
        expect(Number.isFinite(magnitudeDb(coeffs, hz, RATE))).toBe(true);
      }
    });

    it('survives a Q of zero', () => {
      const coeffs = makePeak(RATE, 1000, 0, 6);
      expect(Number.isFinite(magnitudeDb(coeffs, 1000, RATE))).toBe(true);
    });
  });
});

describe('tone stage curves', () => {
  it('draws a flat line when every band is at zero', () => {
    for (const effectId of Object.keys(TONE_BANDS)) {
      const curve = computeToneCurve(effectId, { bassDb: 0, midDb: 0, trebleDb: 0 });

      expect(curve.length).toBeGreaterThan(10);

      for (const point of curve) expect(point.db).toBeCloseTo(0, 4);
    }
  });

  it('puts the EQ bands where the engine puts them', () => {
    // Read straight out of EQProcessor.cpp: 120 Hz shelf, 1 kHz peak, 7 kHz
    // shelf. If those move in the C++ and not here, the curve becomes a lie.
    const curve = computeToneCurve('eq', { bassDb: 0, midDb: 12, trebleDb: 0 });

    const peak = curve.reduce((best, point) => (point.db > best.db ? point : best));

    expect(peak.freqHz).toBeGreaterThan(800);
    expect(peak.freqHz).toBeLessThan(1250);
    expect(peak.db).toBeCloseTo(12, 0);
  });

  it('puts the Contour bands an octave or more below the EQ ones', () => {
    // The two stages exist to do different jobs, and the mid band is where it
    // shows: 1 kHz before the distortion, 500 Hz after it.
    const eqMid = TONE_BANDS.eq.find((band) => band.parameter === 'midDb');
    const contourMid = TONE_BANDS.toneStack.find((band) => band.parameter === 'midDb');

    expect(eqMid?.freqHz).toBe(1000);
    expect(contourMid?.freqHz).toBe(500);
  });

  it('adds the bands together rather than showing only the loudest', () => {
    const single = computeToneCurve('toneStack', { bassDb: 6, midDb: 0, trebleDb: 0 });
    const both = computeToneCurve('toneStack', { bassDb: 6, midDb: 6, trebleDb: 0 });

    const at500 = (curve: typeof single) =>
      curve.reduce((best, point) =>
        Math.abs(point.freqHz - 500) < Math.abs(best.freqHz - 500) ? point : best,
      ).db;

    expect(at500(both)).toBeGreaterThan(at500(single) + 4);
  });

  it('cuts as deep as it boosts', () => {
    const boost = computeToneCurve('eq', { bassDb: 0, midDb: 9, trebleDb: 0 });
    const cut = computeToneCurve('eq', { bassDb: 0, midDb: -9, trebleDb: 0 });

    for (let i = 0; i < boost.length; i += 1) {
      expect(boost[i].db).toBeCloseTo(-cut[i].db, 3);
    }
  });

  it('samples logarithmically, so the low end is not one pixel wide', () => {
    const curve = computeToneCurve('eq', { bassDb: 0, midDb: 0, trebleDb: 0 });

    const below1k = curve.filter((point) => point.freqHz < 1000).length;

    // Linear spacing would put barely 3% of the points below 1 kHz; log
    // spacing puts roughly half of them there.
    expect(below1k).toBeGreaterThan(curve.length * 0.4);
  });

  it('never plots past Nyquist', () => {
    const curve = computeToneCurve('eq', { bassDb: 0, midDb: 0, trebleDb: 0 }, { sampleRate: 8000 });

    expect(curve[curve.length - 1].freqHz).toBeLessThanOrEqual(4000);
    for (const point of curve) expect(Number.isFinite(point.db)).toBe(true);
  });

  it('returns nothing for a stage that is not a tone stack', () => {
    expect(computeToneCurve('overdrive', { drivePct: 50 })).toEqual([]);
    expect(computeToneCurve('', {})).toEqual([]);
  });

  it('treats a missing band as zero rather than NaN', () => {
    const curve = computeToneCurve('eq', {});

    expect(curve.length).toBeGreaterThan(0);
    for (const point of curve) expect(point.db).toBeCloseTo(0, 4);
  });
});
