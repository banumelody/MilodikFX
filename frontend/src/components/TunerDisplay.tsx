import { memo, useEffect, useState } from 'react';

import { setTunerEnabled, subscribeTuner } from '../services/api';
import type { TunerReading } from '../services/api';

/** Within this many cents the note counts as in tune and the display goes green. */
const IN_TUNE_CENTS = 5;

const IDLE: TunerReading = {
  enabled: false,
  note: '',
  midiNote: -1,
  frequency: 0,
  cents: 0,
  confidence: 0,
  detected: false,
};

interface TunerDisplayProps {
  disabled?: boolean;
}

/**
 * Chromatic tuner.
 *
 * Analysis costs a background thread's worth of work in the engine, so the
 * panel switches it on when opened and off when closed rather than leaving it
 * running behind a collapsed card.
 */
function TunerDisplayBase({ disabled = false }: TunerDisplayProps) {
  const [open, setOpen] = useState(false);
  const [reading, setReading] = useState<TunerReading>(IDLE);

  useEffect(() => {
    if (!open) return undefined;

    let cancelled = false;

    void setTunerEnabled(true).catch(() => {
      /* the poll below will report the engine as unreachable soon enough */
    });

    const unsubscribe = subscribeTuner(
      (next) => {
        if (!cancelled) setReading(next);
      },
      () => {
        if (!cancelled) setReading(IDLE);
      },
    );

    return () => {
      cancelled = true;
      unsubscribe();
      void setTunerEnabled(false).catch(() => {
        /* closing the panel is best-effort; a dead engine has stopped anyway */
      });
      setReading(IDLE);
    };
  }, [open]);

  const detected = open && reading.detected;
  const cents = detected ? reading.cents : 0;
  const inTune = detected && Math.abs(cents) <= IN_TUNE_CENTS;

  // -50..+50 cents across the full width of the scale.
  const needlePercent = 50 + Math.max(-50, Math.min(50, cents));

  const state = !detected ? 'idle' : inTune ? 'in-tune' : cents < 0 ? 'flat' : 'sharp';

  return (
    <section className={`panel tuner tuner--${state}`} aria-label="Tuner">
      <header className="panel__head">
        <h2 className="panel__title">Tuner</h2>
        <button
          type="button"
          className={`pill-btn${open ? ' pill-btn--active' : ''}`}
          disabled={disabled}
          aria-pressed={open}
          onClick={() => setOpen((current) => !current)}
        >
          {open ? 'Berhenti' : 'Mulai'}
        </button>
      </header>

      {open ? (
        <>
          <div
            className="tuner__note"
            role="status"
            aria-live="polite"
            aria-label={
              detected
                ? `${reading.note}, ${cents > 0 ? '+' : ''}${cents.toFixed(0)} cent`
                : 'Tidak ada nada terdeteksi'
            }
          >
            {detected ? reading.note : '--'}
          </div>

          <div
            className="tuner__scale"
            role="meter"
            aria-label="Deviasi dalam cent"
            aria-valuemin={-50}
            aria-valuemax={50}
            aria-valuenow={Math.round(cents)}
          >
            <span className="tuner__tick tuner__tick--edge" />
            <span className="tuner__tick tuner__tick--centre" />
            <span className="tuner__tick tuner__tick--edge" />
            {detected ? (
              <span className="tuner__needle" style={{ left: `${needlePercent}%` }} />
            ) : null}
          </div>

          <dl className="tuner__readout">
            <div>
              <dt>Frekuensi</dt>
              <dd>{detected ? `${reading.frequency.toFixed(2)} Hz` : '--'}</dd>
            </div>
            <div>
              <dt>Deviasi</dt>
              <dd>
                {detected ? `${cents > 0 ? '+' : ''}${cents.toFixed(1)} cent` : '--'}
              </dd>
            </div>
          </dl>

          <p className="panel__hint">
            {detected
              ? inTune
                ? 'Pas.'
                : cents < 0
                  ? 'Terlalu rendah — kencangkan.'
                  : 'Terlalu tinggi — kendurkan.'
              : 'Petik satu senar saja, biarkan berdering.'}
          </p>
        </>
      ) : (
        <p className="panel__hint">
          Tuner mati agar tidak membebani CPU. Tekan Mulai saat perlu menyetem.
        </p>
      )}
    </section>
  );
}

export const TunerDisplay = memo(TunerDisplayBase);

export default TunerDisplay;
