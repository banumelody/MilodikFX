import { memo } from 'react';
import { useT } from '../i18n';
import type { StringKey } from '../i18n';

import { Knob } from './Knob';
import { recordKey, useLooper } from '../hooks/useLooper';
import type { LooperInfo, LooperState } from '../services/api';

/** Keyed rather than worded, because the words depend on the chosen language. */
const STATE_KEYS: Record<LooperState, StringKey> = {
  empty: 'looper.empty',
  recording: 'looper.recording',
  playing: 'looper.play',
  overdubbing: 'looper.overdub',
  stopped: 'looper.stop',
};

export interface LooperPanelProps {
  disabled?: boolean;
  /**
   * The looper as reported in the meter stream. Passing it means this panel
   * needs no connection of its own; leaving it out falls back to a poll.
   */
  streamed?: LooperInfo;
}

/**
 * A single-loop phrase looper, mixed in after the master stage so it keeps
 * playing across a global bypass. One Record button carries the whole cycle
 * (record → close → overdub/play); Stop and Clear are explicit. Bind a footswitch
 * to it from the MIDI panel to run it hands-free.
 */
function LooperPanelBase({ disabled = false, streamed }: LooperPanelProps) {
  const t = useT();

  const { info, act, setLevel } = useLooper(!disabled, streamed);

  const state = info?.state ?? 'empty';
  const busy = disabled || info == null;
  const recording = state === 'recording' || state === 'overdubbing';
  const seconds = info?.loopSeconds ?? 0;

  return (
    <section className="panel looper" aria-label="Looper">
      <header className="panel__head">
        <h2 className="panel__title">{t('looper.title')}</h2>
        <span className={`looper__state looper__state--${state}`}>{t(STATE_KEYS[state])}</span>
      </header>

      <p className="panel__hint">
        {t('looper.hintBefore')} <b>{t(recordKey(state))}</b> {t('looper.hintAfter')}
      </p>

      <div
        className="looper__progress"
        role="progressbar"
        aria-label={t('looper.position')}
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
        <span>
          {info?.hasLoop ? t('looper.seconds', { n: seconds.toFixed(1) }) : t('looper.noLoop')}
        </span>
        <span className="muted">{t('looper.max', { n: Math.round(info?.maxSeconds ?? 60) })}</span>
      </div>

      <div className="looper__controls">
        <button
          type="button"
          className={`btn looper__rec${recording ? ' looper__rec--on' : ''}`}
          disabled={busy}
          onClick={() => act('record')}
        >
          {t(recordKey(state))}
        </button>
        <button
          type="button"
          className="btn btn--ghost"
          disabled={busy || state === 'empty'}
          onClick={() => act('stop')}
        >
          {t('looper.stop')}
        </button>
        <button
          type="button"
          className="btn btn--ghost"
          disabled={busy || !info?.hasLoop}
          onClick={() => act('clear')}
        >
          {t('looper.clear')}
        </button>
      </div>

      <div className="looper__level">
        <Knob
          value={info?.level ?? 100}
          min={0}
          max={100}
          step={1}
          defaultValue={100}
          label={t('looper.level')}
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
