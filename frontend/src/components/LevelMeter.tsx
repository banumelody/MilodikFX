import { useEffect, useRef, useState } from 'react';

export interface LevelMeterProps {
  label: string;
  /** Level in dBFS. */
  db: number;
  /** Below this the readout shows a dash rather than a noise-floor number. */
  silenceDb?: number;
  /** Bottom of the *displayed* scale; the engine's floor stays at -100. */
  floorDb?: number;
  /** Top of the scale in dB; headroom above 0 makes clipping visible. */
  ceilingDb?: number;
  /**
   * The level before any trim was applied, when that differs.
   *
   * Only used to warn. Trimming a hot input down makes the bar look healthy
   * while the converter is still clipping, and no digital trim can undo that --
   * so the warning has to come from the untrimmed figure.
   */
  sourceDb?: number;
}

/** How long a peak marker stays before it starts falling, in ms. */
const PEAK_HOLD_MS = 1200;

/** How fast the peak marker falls once it lets go, in dB per second. */
const PEAK_FALL_DB_PER_SEC = 20;

/**
 * A read-only meter.
 *
 * The scale starts at -60 rather than the engine's -100 floor: at full range an
 * idle noise floor around -77 dB already drew a fifth of the bar, so the meter
 * looked busy in silence and squeezed the range that matters into the far right.
 */
export function LevelMeter({
  label,
  db,
  silenceDb = -65,
  floorDb = -60,
  ceilingDb = 6,
  sourceDb,
}: LevelMeterProps) {
  const span = ceilingDb - floorDb;
  const clamped = Math.min(ceilingDb, Math.max(floorDb, db));
  const ratio = (clamped - floorDb) / span;

  const zeroRatio = (0 - floorDb) / span;
  const sourceClipping = sourceDb !== undefined && sourceDb > -0.5;
  const isHot = db > -1 || sourceClipping;
  const isLoud = db > -12;

  const [peakDb, setPeakDb] = useState(floorDb);
  const peakRef = useRef({ value: floorDb, heldSince: 0 });

  useEffect(() => {
    const now = performance.now();
    const state = peakRef.current;

    if (db >= state.value) {
      state.value = db;
      state.heldSince = now;
      setPeakDb(db);
      return;
    }

    if (now - state.heldSince < PEAK_HOLD_MS) return;

    const elapsed = (now - state.heldSince - PEAK_HOLD_MS) / 1000;
    const fallen = Math.max(db, state.value - elapsed * PEAK_FALL_DB_PER_SEC);

    state.value = fallen;
    state.heldSince = now - PEAK_HOLD_MS;
    setPeakDb(fallen);
  }, [db]);

  // "CLIP" wins over the number: a trimmed-down reading looks perfectly healthy
  // and would hide the one problem the trim cannot solve.
  const readout = sourceClipping ? 'CLIP' : db <= silenceDb ? '--' : `${db.toFixed(1)} dB`;
  const peakRatio = (Math.min(ceilingDb, Math.max(floorDb, peakDb)) - floorDb) / span;
  const showPeak = peakDb > silenceDb;

  return (
    <div className="meter">
      <div className="meter__head">
        <span className="meter__label">{label}</span>
        <span className={`meter__value${isHot ? ' meter__value--hot' : ''}`}>{readout}</span>
      </div>
      <div
        className="meter__track"
        role="meter"
        aria-label={label}
        aria-valuemin={floorDb}
        aria-valuemax={ceilingDb}
        aria-valuenow={Number(clamped.toFixed(1))}
        aria-valuetext={readout}
      >
        <div
          className={`meter__fill${isHot ? ' meter__fill--hot' : isLoud ? ' meter__fill--loud' : ''}`}
          style={{ width: `${ratio * 100}%` }}
        />
        <div className="meter__zero" style={{ left: `${zeroRatio * 100}%` }} />
        {showPeak ? (
          <div
            className={`meter__peak${peakDb > -1 ? ' meter__peak--hot' : ''}`}
            style={{ left: `${peakRatio * 100}%` }}
          />
        ) : null}
      </div>
    </div>
  );
}

export interface ReductionMeterProps {
  label: string;
  /** Reduction in dB, zero or negative. */
  db: number;
  maxDb?: number;
}

export function ReductionMeter({ label, db, maxDb = 24 }: ReductionMeterProps) {
  const amount = Math.min(maxDb, Math.max(0, -db));
  const ratio = amount / maxDb;

  return (
    <div className="meter meter--reduction">
      <div className="meter__head">
        <span className="meter__label">{label}</span>
        <span className="meter__value">{amount < 0.05 ? '--' : `-${amount.toFixed(1)} dB`}</span>
      </div>
      <div className="meter__track">
        <div className="meter__fill meter__fill--reduction" style={{ width: `${ratio * 100}%` }} />
      </div>
    </div>
  );
}

export default LevelMeter;
