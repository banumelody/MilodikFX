import { useCallback, useEffect, useRef, useState } from 'react';

import { getLooper, looperAction, setLooperLevel } from '../services/api';
import type { LooperAction, LooperInfo } from '../services/api';

/**
 * Subscribes to the looper's state while `active`, and hands back the two ways to
 * drive it. Polls rather than streams: the state changes rarely (a footswitch, a
 * button) and the payload is tiny, so a light poll is simpler than another SSE
 * connection -- and it catches a footswitch-driven change the UI did not make.
 *
 * A press returns the engine's new state, so the button lights immediately rather
 * than waiting for the next poll.
 */
export function useLooper(active: boolean, intervalMs = 250) {
  const [info, setInfo] = useState<LooperInfo | null>(null);
  const inFlight = useRef(false);

  const refresh = useCallback(async () => {
    if (inFlight.current) return;
    inFlight.current = true;
    try {
      setInfo(await getLooper());
    } catch {
      /* the connection banner already reports an unreachable engine */
    } finally {
      inFlight.current = false;
    }
  }, []);

  useEffect(() => {
    if (!active) return undefined;
    void refresh();
    const timer = window.setInterval(() => void refresh(), intervalMs);
    return () => window.clearInterval(timer);
  }, [active, intervalMs, refresh]);

  const act = useCallback((action: LooperAction) => {
    void (async () => {
      try {
        setInfo(await looperAction(action));
      } catch {
        /* ignore; the poll will resync */
      }
    })();
  }, []);

  const setLevel = useCallback((value: number) => {
    // Optimistic, like a knob: the poll is authoritative on the next tick.
    setInfo((current) => (current ? { ...current, level: value } : current));
    void setLooperLevel(value).catch(() => {});
  }, []);

  return { info, act, setLevel, refresh };
}

/** What pressing the context-sensitive Record button does next, given the state. */
export function recordLabel(state: LooperInfo['state'] | undefined): string {
  switch (state) {
    case 'recording':
      return 'Tutup loop';
    case 'playing':
      return 'Overdub';
    case 'overdubbing':
      return 'Selesai overdub';
    case 'stopped':
      return 'Main';
    case 'empty':
    default:
      return 'Rekam';
  }
}
