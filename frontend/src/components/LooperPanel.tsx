import { memo } from 'react';

import { Knob } from './Knob';
import { recordLabel, useLooper } from '../hooks/useLooper';
import type { LooperState } from '../services/api';

const STATE_LABELS: Record<LooperState, string> = {
  empty: 'Kosong',
  recording: 'Merekam',
  playing: 'Main',
  overdubbing: 'Overdub',
  stopped: 'Berhenti',
};

export interface LooperPanelProps {
  disabled?: boolean;
}

/**
 * A single-loop phrase looper, mixed in after the master stage so it keeps
 * playing across a global bypass. One Record button carries the whole cycle
 * (rekam → tutup → overdub/main); Stop and Hapus are explicit. Bind a footswitch
 * to it from the MIDI panel to run it hands-free.
 */
function LooperPanelBase({ disabled = false }: LooperPanelProps) {
  const { info, act, setLevel } = useLooper(!disabled);

  const state = info?.state ?? 'empty';
  const busy = disabled || info == null;
  const recording = state === 'recording' || state === 'overdubbing';
  const seconds = info?.loopSeconds ?? 0;

  return (
    <section className="panel looper" aria-label="Looper">
      <header className="panel__head">
        <h2 className="panel__title">Looper</h2>
        <span className={`looper__state looper__state--${state}`}>{STATE_LABELS[state]}</span>
      </header>

      <p className="panel__hint">
        Rekam satu frasa lalu bermain di atasnya. Tombol <b>{recordLabel(state)}</b> menjalankan
        siklusnya; loop tetap berbunyi walau global bypass menyala. Petakan footswitch dari panel MIDI
        untuk kendali kaki.
      </p>

      <div
        className="looper__progress"
        role="progressbar"
        aria-label="Posisi loop"
        aria-valuemin={0}
        aria-valuemax={100}
        aria-valuenow={Math.round((info?.position ?? 0) * 100)}
      >
        <span
          className={`looper__bar${recording ? ' looper__bar--rec' : ''}`}
          style={{ width: `${Math.min(100, Math.max(0, (info?.position ?? 0) * 100))}%` }}
        />
      </div>

      <div className="looper__meta">
        <span>{info?.hasLoop ? `${seconds.toFixed(1)} d` : 'Belum ada loop'}</span>
        <span className="muted">maks {Math.round(info?.maxSeconds ?? 60)} d</span>
      </div>

      <div className="looper__controls">
        <button
          type="button"
          className={`btn looper__rec${recording ? ' looper__rec--on' : ''}`}
          disabled={busy}
          onClick={() => act('record')}
        >
          {recordLabel(state)}
        </button>
        <button
          type="button"
          className="btn btn--ghost"
          disabled={busy || state === 'empty'}
          onClick={() => act('stop')}
        >
          Stop
        </button>
        <button
          type="button"
          className="btn btn--ghost"
          disabled={busy || !info?.hasLoop}
          onClick={() => act('clear')}
        >
          Hapus
        </button>
      </div>

      <div className="looper__level">
        <Knob
          value={info?.level ?? 100}
          min={0}
          max={100}
          step={1}
          defaultValue={100}
          label="Level"
          unit="%"
          size={64}
          disabled={busy}
          format={(value) => value.toFixed(0)}
          onChange={setLevel}
        />
      </div>
    </section>
  );
}

export const LooperPanel = memo(LooperPanelBase);

export default LooperPanel;
