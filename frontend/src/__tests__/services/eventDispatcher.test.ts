import { describe, it, expect, vi, beforeEach } from 'vitest';
import { EventDispatcher } from '../../services/eventDispatcher';

describe('EventDispatcher Service', () => {
  let dispatcher: EventDispatcher;

  beforeEach(() => {
    dispatcher = new EventDispatcher();
  });

  it('registers event listeners', () => {
    const callback = vi.fn();
    dispatcher.on('test', callback);

    expect(dispatcher.listenerCount('test')).toBe(1);
  });

  it('emits events to registered listeners', () => {
    const callback = vi.fn();
    dispatcher.on('test', callback);
    dispatcher.emit('test', { data: 'hello' });

    expect(callback).toHaveBeenCalledWith({ data: 'hello' });
  });

  it('removes event listeners with off', () => {
    const callback = vi.fn();
    dispatcher.on('test', callback);
    dispatcher.off('test', callback);
    dispatcher.emit('test', {});

    expect(callback).not.toHaveBeenCalled();
  });

  it('handles once listeners', () => {
    const callback = vi.fn();
    dispatcher.once('test', callback);

    dispatcher.emit('test', { data: '1' });
    dispatcher.emit('test', { data: '2' });

    expect(callback).toHaveBeenCalledTimes(1);
    expect(callback).toHaveBeenCalledWith({ data: '1' });
  });

  it('handles multiple listeners for same event', () => {
    const callback1 = vi.fn();
    const callback2 = vi.fn();

    dispatcher.on('test', callback1);
    dispatcher.on('test', callback2);
    dispatcher.emit('test', {});

    expect(callback1).toHaveBeenCalledOnce();
    expect(callback2).toHaveBeenCalledOnce();
  });

  it('clears specific event listeners', () => {
    const callback1 = vi.fn();
    const callback2 = vi.fn();

    dispatcher.on('event1', callback1);
    dispatcher.on('event2', callback2);

    dispatcher.clear('event1');

    dispatcher.emit('event1', {});
    dispatcher.emit('event2', {});

    expect(callback1).not.toHaveBeenCalled();
    expect(callback2).toHaveBeenCalledOnce();
  });

  it('clears all listeners when no event specified', () => {
    const callback1 = vi.fn();
    const callback2 = vi.fn();

    dispatcher.on('event1', callback1);
    dispatcher.on('event2', callback2);

    dispatcher.clear();

    dispatcher.emit('event1', {});
    dispatcher.emit('event2', {});

    expect(callback1).not.toHaveBeenCalled();
    expect(callback2).not.toHaveBeenCalled();
  });
});
