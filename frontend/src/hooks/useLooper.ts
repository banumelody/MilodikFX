import { useCallback, useEffect, useRef, useState } from 'react';

import { getLooper, looperAction, setLooperLevel } from '../services/api';
import type { LooperAction, LooperInfo } from '../services/api';
import type { StringKey } from '../i18n';

/** Field-by-field, so an unchanged looper does not re-render the panel. */
function sameLooper(a: LooperInfo | null, b: LooperInfo): boolean {
  return (
    a != null &&
    a.state === b.state &&
    a.hasLoop === b.hasLoop &&
    a.loopSeconds === b.loopSeconds &&
    a.position === b.position &&
    a.level === b.level &&
    a.maxSeconds === b.maxSeconds
  );
}

/**
 * Subscribes to the looper's state while `active`, and hands back the two ways to
 * drive it.
 *
 * The engine reports the looper inside the meter payload, which is already
 * streaming ~22 times a second, so `streamed` is normally all this needs -- no
 * connection and no poll of its own. It used to ask /api/looper four times a
 * second, and on a thread-per-connection server that is four sockets a second
 * for something that fits in a stream already going out.
 *
 * An engine that does not report it there still works: pass nothing and the old
 * poll takes over. Same graceful fallback the level stream itself has.
 */
export function useLooper(active: boolean, streamed?: LooperInfo, intervalMs = 250) {
  const [info, setInfo] = useState<LooperInfo | null>(streamed ?? null);
  const inFlight = useRef(false);
  const streaming = streamed !== undefined;

  // A new object arrives on every stream frame, so this effect runs at meter
  // rate. The comparison is what keeps that from becoming a render at meter
  // rate: an idle looper says exactly the same thing every time.
  useEffect(() => {
    if (!streamed) return;
    setInfo((current) => (sameLooper(current, streamed) ? current : streamed));
  }, [streamed]);

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
    if (!active || streaming) return undefined;
    void refresh();
    const timer = window.setInterval(() => void refresh(), intervalMs);
    return () => window.clearInterval(timer);
  }, [active, streaming, intervalMs, refresh]);

  const act = useCallback((action: LooperAction) => {
    void (async () => {
      try {
        setInfo(await looperAction(action));
      } catch {
        /* ignore; the stream resyncs on the next frame */
      }
    })();
  }, []);

  const setLevel = useCallback((value: number) => {
    // Optimistic, like a knob: the stream is authoritative on the next frame.
    setInfo((current) => (current ? { ...current, level: value } : current));
    void setLooperLevel(value).catch(() => {});
  }, []);

  return { info, act, setLevel, refresh };
}

/**
 * What pressing the context-sensitive Record button does next, given the state.
 *
 * A key rather than a word: the button is rendered in whichever language the app
 * is set to, and a hook has no business knowing which that is.
 */
export function recordKey(state: LooperInfo['state'] | undefined): StringKey {
  switch (state) {
    case 'recording':
      return 'looper.closeLoop';
    case 'playing':
      return 'looper.overdub';
    case 'overdubbing':
      return 'looper.finishOverdub';
    case 'stopped':
      return 'looper.play';
    case 'empty':
    default:
      return 'looper.record';
  }
}
