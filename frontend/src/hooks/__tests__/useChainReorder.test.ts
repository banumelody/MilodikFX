import { describe, expect, it, vi } from 'vitest';
import { act, renderHook } from '@testing-library/react';

import { reorderBy, reorderByDelta, useChainReorder } from '../useChainReorder';

const ORDER = ['input', 'noiseGate', 'overdrive', 'eq', 'delay', 'master'];
const FIXED = ['input', 'master'];

describe('reorderBy', () => {
  it('moves a stage to where the target sits', () => {
    expect(reorderBy(ORDER, 'overdrive', 'noiseGate')).toEqual([
      'input',
      'overdrive',
      'noiseGate',
      'eq',
      'delay',
      'master',
    ]);
  });

  it('moves a stage later as well as earlier', () => {
    expect(reorderBy(ORDER, 'noiseGate', 'delay')).toEqual([
      'input',
      'overdrive',
      'eq',
      'delay',
      'noiseGate',
      'master',
    ]);
  });

  it('returns the same array when nothing would change', () => {
    // Identity matters: the caller uses it to decide whether to ask the engine
    // at all, so a no-op drag must not cost a round trip.
    expect(reorderBy(ORDER, 'overdrive', 'overdrive')).toBe(ORDER);
    expect(reorderBy(ORDER, 'nope', 'eq')).toBe(ORDER);
  });
});

describe('reorderByDelta', () => {
  it('steps one place in either direction', () => {
    expect(reorderByDelta(ORDER, 'overdrive', -1)[1]).toBe('overdrive');
    expect(reorderByDelta(ORDER, 'overdrive', 1)[3]).toBe('overdrive');
  });

  it('refuses to walk off either end', () => {
    expect(reorderByDelta(ORDER, 'input', -1)).toBe(ORDER);
    expect(reorderByDelta(ORDER, 'master', 1)).toBe(ORDER);
  });
});

describe('useChainReorder', () => {
  const setup = () => {
    const onReorder = vi.fn();
    const view = renderHook(() => useChainReorder({ order: ORDER, fixed: FIXED, onReorder }));
    return { onReorder, view };
  };

  it('offers no drag affordance for a pinned stage', () => {
    const { view } = setup();

    // Not focusable and not grabbable: the engine would refuse it anyway, and a
    // control that looks draggable but never works is worse than none.
    expect(view.result.current.handleProps('master').tabIndex).toBe(-1);
    expect(view.result.current.handleProps('overdrive').tabIndex).toBe(0);
  });

  it('lifts a stage with Enter and moves it with the arrows', () => {
    const { onReorder, view } = setup();

    const press = (key: string) =>
      act(() => {
        view.result.current
          .handleProps('overdrive')
          .onKeyDown({ key, preventDefault: vi.fn() } as never);
      });

    press('Enter');
    expect(view.result.current.state.lifted).toBe(true);
    expect(view.result.current.state.activeId).toBe('overdrive');

    press('ArrowUp');
    expect(onReorder).toHaveBeenCalledWith([
      'input',
      'overdrive',
      'noiseGate',
      'eq',
      'delay',
      'master',
    ]);
  });

  it('ignores the arrows until a stage is actually lifted', () => {
    const { onReorder, view } = setup();

    act(() => {
      view.result.current
        .handleProps('overdrive')
        .onKeyDown({ key: 'ArrowUp', preventDefault: vi.fn() } as never);
    });

    expect(onReorder).not.toHaveBeenCalled();
  });

  it('will not push a stage onto a pinned one', () => {
    const { onReorder, view } = setup();

    const press = (key: string) =>
      act(() => {
        view.result.current
          .handleProps('noiseGate')
          .onKeyDown({ key, preventDefault: vi.fn() } as never);
      });

    press('Enter');
    // noiseGate sits directly after the pinned input, so moving it up would put
    // it where input has to be. Refused here rather than by the engine, so the
    // card never jumps and comes back.
    press('ArrowUp');

    expect(onReorder).not.toHaveBeenCalled();
  });

  it('drops the lift on Escape without reordering', () => {
    const { onReorder, view } = setup();

    const press = (key: string) =>
      act(() => {
        view.result.current
          .handleProps('overdrive')
          .onKeyDown({ key, preventDefault: vi.fn() } as never);
      });

    press('Enter');
    press('Escape');

    expect(view.result.current.state.lifted).toBe(false);
    expect(view.result.current.state.activeId).toBeNull();
    expect(onReorder).not.toHaveBeenCalled();
  });

  it('clears its state after a drag so nothing is left highlighted', () => {
    const { view } = setup();

    act(() => {
      view.result.current
        .handleProps('overdrive')
        .onKeyDown({ key: 'Enter', preventDefault: vi.fn() } as never);
    });
    expect(view.result.current.state.activeId).toBe('overdrive');

    act(() => {
      view.result.current
        .handleProps('overdrive')
        .onKeyDown({ key: 'Enter', preventDefault: vi.fn() } as never);
    });

    expect(view.result.current.state).toEqual({
      activeId: null,
      overId: null,
      dragging: false,
      lifted: false,
    });
  });
});
