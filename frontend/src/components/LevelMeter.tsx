export interface LevelMeterProps {
  label: string;
  /** Level in dBFS. */
  db: number;
  floorDb?: number;
  /** Top of the scale in dB; headroom above 0 makes clipping visible. */
  ceilingDb?: number;
}

/**
 * A read-only meter.
 *
 * The old UI drew levels with range inputs, so the "meters" were draggable and
 * fought the polling loop for control of their own value.
 */
export function LevelMeter({ label, db, floorDb = -72, ceilingDb = 6 }: LevelMeterProps) {
  const span = ceilingDb - floorDb;
  const clamped = Math.min(ceilingDb, Math.max(floorDb, db));
  const ratio = (clamped - floorDb) / span;

  const zeroRatio = (0 - floorDb) / span;
  const isHot = db > -1;
  const isLoud = db > -12;

  const readout = db <= floorDb ? '--' : `${db.toFixed(1)} dB`;

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
