import { fireEvent, render, screen } from '@testing-library/react';
import { describe, expect, it, vi } from 'vitest';

import { DeviceSettings } from '../DeviceSettings';
import type { DevicesResponse } from '../../services/api';

/** A Scarlett 4i4: two instrument jacks on the front, two line jacks on the back. */
function scarlett(overrides: Partial<DevicesResponse> = {}): DevicesResponse {
  return {
    current: {
      open: true,
      type: 'ASIO',
      inputDevice: 'Focusrite USB ASIO',
      outputDevice: 'Focusrite USB ASIO',
      sampleRate: 96000,
      bufferSize: 32,
      inputChannels: 4,
      outputChannels: 4,
      inputLatencyMs: 2.1,
      outputLatencyMs: 2.2,
      roundTripLatencyMs: 4.3,
      lowLatency: true,
    },
    available: {
      types: [{ name: 'ASIO', lowLatency: true, inputs: ['Focusrite USB ASIO'], outputs: ['Focusrite USB ASIO'] }],
      currentType: 'ASIO',
      availableSampleRates: [48000, 96000],
      availableBufferSizes: [32, 64],
    },
    inputRouting: {
      ports: [
        { name: 'Input 1', available: true },
        { name: 'Input 2', available: true },
        { name: 'Input 3', available: true },
        { name: 'Input 4', available: false },
      ],
      left: 'Input 1',
      right: 'Input 2',
    },
    ...overrides,
  };
}

/** The port selectors live behind the "Ubah" toggle, like every other control. */
function openForm() {
  fireEvent.click(screen.getByRole('button', { name: /ubah/i }));
}

describe('DeviceSettings input routing', () => {
  it('offers every physical jack for each engine channel', () => {
    render(
      <DeviceSettings
        devices={scarlett()}
        busy={false}
        error={null}
        onApply={() => {}}
        onRefresh={() => {}}
        onOptimise={() => {}}
      />,
    );
    openForm();

    const left = screen.getByLabelText(/kanal l dari/i) as HTMLSelectElement;
    const right = screen.getByLabelText(/kanal r dari/i) as HTMLSelectElement;

    expect(left.value).toBe('Input 1');
    expect(right.value).toBe('Input 2');

    // Four jacks plus the automatic entry. Before v0.29 the engine only ever
    // opened two channels, so 3 and 4 could not be reached at all.
    expect(left.querySelectorAll('option')).toHaveLength(5);
    expect(screen.getAllByRole('option', { name: /^Input 3$/ })).toHaveLength(2);
  });

  it('marks a jack the device is not streaming as unselectable', () => {
    render(
      <DeviceSettings
        devices={scarlett()}
        busy={false}
        error={null}
        onApply={() => {}}
        onRefresh={() => {}}
        onOptimise={() => {}}
      />,
    );
    openForm();

    const [option] = screen.getAllByRole('option', { name: /Input 4 - tidak aktif/ });
    expect(option).toBeDisabled();
  });

  it('sends only the side that changed', () => {
    const onApply = vi.fn();

    render(
      <DeviceSettings
        devices={scarlett()}
        busy={false}
        error={null}
        onApply={onApply}
        onRefresh={() => {}}
        onOptimise={() => {}}
      />,
    );
    openForm();

    fireEvent.change(screen.getByLabelText(/kanal r dari/i), { target: { value: 'Input 3' } });

    // Not both: the engine keeps whichever side the body leaves out, so sending
    // a stale value for the other channel would undo a concurrent change.
    expect(onApply).toHaveBeenCalledWith({ inputPortRight: 'Input 3' });
  });

  it('lets a channel fall back to automatic', () => {
    const onApply = vi.fn();

    render(
      <DeviceSettings
        devices={scarlett()}
        busy={false}
        error={null}
        onApply={onApply}
        onRefresh={() => {}}
        onOptimise={() => {}}
      />,
    );
    openForm();

    fireEvent.change(screen.getByLabelText(/kanal l dari/i), { target: { value: '' } });
    expect(onApply).toHaveBeenCalledWith({ inputPortLeft: '' });
  });

  it('hides the selectors on an interface with a single input', () => {
    const mono = scarlett({
      inputRouting: { ports: [{ name: 'Microphone', available: true }], left: '', right: '' },
    });

    render(
      <DeviceSettings
        devices={mono}
        busy={false}
        error={null}
        onApply={() => {}}
        onRefresh={() => {}}
        onOptimise={() => {}}
      />,
    );
    openForm();

    // Nothing to choose between, so the control would be noise.
    expect(screen.queryByLabelText(/kanal l dari/i)).not.toBeInTheDocument();
  });

  it('survives an engine that reports no routing at all', () => {
    // An older engine, or one whose device is not open yet.
    const withoutRouting = scarlett();
    delete (withoutRouting as Partial<DevicesResponse>).inputRouting;

    render(
      <DeviceSettings
        devices={withoutRouting as DevicesResponse}
        busy={false}
        error={null}
        onApply={() => {}}
        onRefresh={() => {}}
        onOptimise={() => {}}
      />,
    );
    openForm();

    expect(screen.queryByLabelText(/kanal l dari/i)).not.toBeInTheDocument();
    expect(screen.getByLabelText(/sample rate/i)).toBeInTheDocument();
  });
});
