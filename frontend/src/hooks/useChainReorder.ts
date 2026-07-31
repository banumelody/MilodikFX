import { useCallback, useMemo, useRef, useState } from 'react';

/**
 * Drag-to-reorder for the signal chain, shared by the rack and the chain strip.
 *
 * **Pointer events, not the HTML5 drag-and-drop API.** That API is awkward
 * inside WebView2, cannot be driven in jsdom, and gives no control over the drag
 * image. Pointer capture is the same discipline `Knob` already uses here, so the
 * quirks are ones this project has already paid for.
 *
 * Nothing is reordered while the pointer moves. The engine is asked once, on
 * release, and the rack redraws from what it answers -- so a refusal (a pinned
 * stage) simply leaves everything where it was, with no local state to unwind.
 *
 * A keyboard path runs alongside it rather than behind it: Enter or Space lifts
 * a stage, the arrows move it, Enter drops it and Escape puts it back. Anyone
 * who cannot drag gets the same feature, not a lesser one.
 */
export interface ChainReorderOptions {
  /** Current order, as effect ids. */
  order: string[];
  /** Ids the engine will not move. Dragging them is refused up front. */
  fixed: string[];
  /** Called with the whole new order once a drag or a keyboard move commits. */
  onReorder: (next: string[]) => void;
}

export interface DropState {
  /** The id being dragged or lifted, or null. */
  activeId: string | null;
  /** The id the pointer is currently over. */
  overId: string | null;
  /** True while a pointer drag is in progress (as opposed to a keyboard lift). */
  dragging: boolean;
  /** True while a stage is lifted by keyboard. */
  lifted: boolean;
}

/** Moves `id` to sit where `targetId` currently is. */
export function reorderBy(order: string[], id: string, targetId: string): string[] {
  const from = order.indexOf(id);
  const to = order.indexOf(targetId);

  if (from < 0 || to < 0 || from === to) return order;

  const next = [...order];
  next.splice(to, 0, ...next.splice(from, 1));
  return next;
}

/** Moves `id` by `delta` places, clamped to the ends. */
export function reorderByDelta(order: string[], id: string, delta: number): string[] {
  const from = order.indexOf(id);
  if (from < 0) return order;

  const to = from + delta;
  if (to < 0 || to >= order.length) return order;

  const next = [...order];
  next.splice(to, 0, ...next.splice(from, 1));
  return next;
}

export function useChainReorder({ order, fixed, onReorder }: ChainReorderOptions) {
  const [state, setState] = useState<DropState>({
    activeId: null,
    overId: null,
    dragging: false,
    lifted: false,
  });

  // Rects are measured once when a drag starts. Nothing reflows during the drag
  // -- the cards do not move until the engine answers -- so re-measuring on
  // every pointermove would only cost time.
  const rects = useRef<Array<{ id: string; rect: DOMRect }>>([]);
  const fixedSet = useMemo(() => new Set(fixed), [fixed]);

  const measure = useCallback(() => {
    const nodes = document.querySelectorAll<HTMLElement>('[data-chain-stage]');
    rects.current = Array.from(nodes)
      .map((node) => ({ id: node.dataset.chainStage ?? '', rect: node.getBoundingClientRect() }))
      .filter((entry) => entry.id !== '');
  }, []);

  const idAtPoint = useCallback((x: number, y: number) => {
    for (const { id, rect } of rects.current) {
      if (x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom) return id;
    }
    return null;
  }, []);

  const cancel = useCallback(() => {
    setState({ activeId: null, overId: null, dragging: false, lifted: false });
  }, []);

  const commit = useCallback(
    (id: string, targetId: string | null) => {
      if (targetId && targetId !== id && !fixedSet.has(targetId)) {
        const next = reorderBy(order, id, targetId);
        if (next !== order) onReorder(next);
      }
      cancel();
    },
    [order, fixedSet, onReorder, cancel],
  );

  /** Props for the drag handle of one stage. */
  const handleProps = useCallback(
    (id: string) => {
      const movable = !fixedSet.has(id);

      return {
        onPointerDown: (event: React.PointerEvent<HTMLElement>) => {
          // Left button only, and never a stage the engine pins.
          if (!movable || event.button !== 0) return;

          event.preventDefault();
          measure();
          event.currentTarget.setPointerCapture(event.pointerId);
          setState({ activeId: id, overId: id, dragging: true, lifted: false });
        },

        onPointerMove: (event: React.PointerEvent<HTMLElement>) => {
          setState((current) => {
            if (!current.dragging || current.activeId !== id) return current;

            const over = idAtPoint(event.clientX, event.clientY);
            return over === current.overId ? current : { ...current, overId: over };
          });
        },

        onPointerUp: (event: React.PointerEvent<HTMLElement>) => {
          if (!movable) return;

          const over = idAtPoint(event.clientX, event.clientY);

          if (event.currentTarget.hasPointerCapture(event.pointerId))
            event.currentTarget.releasePointerCapture(event.pointerId);

          commit(id, over);
        },

        onPointerCancel: cancel,

        onKeyDown: (event: React.KeyboardEvent<HTMLElement>) => {
          if (!movable) return;

          const lifted = state.lifted && state.activeId === id;

          if (event.key === 'Enter' || event.key === ' ') {
            event.preventDefault();
            if (lifted) cancel();
            else setState({ activeId: id, overId: id, dragging: false, lifted: true });
            return;
          }

          if (!lifted) return;

          if (event.key === 'Escape') {
            event.preventDefault();
            cancel();
            return;
          }

          // Up/Left move it earlier in the chain, Down/Right later -- the rack is
          // a grid and the strip is a row, so both pairs mean the same thing.
          const delta =
            event.key === 'ArrowUp' || event.key === 'ArrowLeft'
              ? -1
              : event.key === 'ArrowDown' || event.key === 'ArrowRight'
                ? 1
                : 0;

          if (delta === 0) return;

          event.preventDefault();

          const next = reorderByDelta(order, id, delta);

          // Refuse a move that would land on a pinned stage rather than letting
          // the engine reject it and the card jump back.
          const landedOn = next.indexOf(id);
          if (next === order || fixedSet.has(order[landedOn])) return;

          onReorder(next);
        },

        tabIndex: movable ? 0 : -1,
        'aria-grabbed': state.lifted && state.activeId === id ? true : undefined,
      };
    },
    [fixedSet, measure, idAtPoint, commit, cancel, state, order, onReorder],
  );

  return { state, handleProps };
}
