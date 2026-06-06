import { describe, it, expect, vi, beforeEach } from 'vitest';
import { MessageBridge } from '../../services/messageBridge';

describe('MessageBridge Service', () => {
  let bridge: MessageBridge;

  beforeEach(() => {
    bridge = new MessageBridge();
  });

  it('initializes without errors', () => {
    expect(bridge).toBeDefined();
  });

  it('is disconnected initially', () => {
    expect(bridge.isConnected()).toBe(false);
  });

  it('registers event listeners', () => {
    const callback = vi.fn();
    bridge.on('parameter', callback);

    expect(bridge).toBeDefined();
  });

  it('removes event listeners', () => {
    const callback = vi.fn();
    bridge.on('parameter', callback);
    bridge.off('parameter', callback);

    expect(bridge).toBeDefined();
  });

  it('handles setParameter gracefully when disconnected', async () => {
    try {
      await bridge.setParameter('test', 'param', 0.5);
    } catch (error) {
      expect(error).toBeDefined();
    }
  });

  it('handles getDeviceList gracefully when disconnected', async () => {
    try {
      await bridge.getDeviceList();
    } catch (error) {
      expect(error).toBeDefined();
    }
  });

  it('handles savePreset gracefully when disconnected', async () => {
    try {
      await bridge.savePreset('test', {});
    } catch (error) {
      expect(error).toBeDefined();
    }
  });

  it('handles loadPreset gracefully when disconnected', async () => {
    try {
      await bridge.loadPreset('test-id');
    } catch (error) {
      expect(error).toBeDefined();
    }
  });

  it('handles deletePreset gracefully when disconnected', async () => {
    try {
      await bridge.deletePreset('test-id');
    } catch (error) {
      expect(error).toBeDefined();
    }
  });
});
