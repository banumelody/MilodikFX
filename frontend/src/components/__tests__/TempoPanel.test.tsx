import { fireEvent, render, screen } from '@testing-library/react';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { TempoPanel } from '../TempoPanel';
import type { EffectDescriptor, ParameterDescriptor } from '../../services/api';

const bpm: ParameterDescriptor = {
  id: 'bpm',
  label: 'Tempo',
  unit: 'BPM',
  min: 30,
  max: 300,
  step: 1,
  default: 120,
  type: 'float',
  value: 120,
};

function makeMetronome(overrides: Partial<EffectDescriptor> = {}): EffectDescriptor {
  return {
    id: 'metronome',
    label: 'Metronome',
    description: 'Practice click',
    enabled: false,
    toggleable: true,
    parameters: [
      {
        id: 'volumePct',
        label: 'Volume',
        unit: '%',
        min: 0,
        max: 100,
        step: 1,
        default: 50,
        type: 'float',
        value: 50,
      },
      {
        id: 'beatsPerBar',
        label: 'Ketukan/Bar',
        unit: '',
        min: 1,
        max: 12,
        step: 1,
        default: 4,
        type: 'float',
        value: 4,
      },
    ],
    ...overrides,
  };
}

function renderPanel(props: Partial<Parameters<typeof TempoPanel>[0]> = {}) {
  const onParameterChange = vi.fn();
  const onEnabledChange = vi.fn();

  const result = render(
    <TempoPanel
      bpm={bpm}
      metronome={makeMetronome()}
      onParameterChange={onParameterChange}
      onEnabledChange={onEnabledChange}
      {...props}
    />,
  );

  return { ...result, onParameterChange, onEnabledChange };
}

describe('TempoPanel', () => {
  it('shows the tempo the engine reports', () => {
    renderPanel();

    expect(screen.getByLabelText('Tempo dalam BPM')).toHaveTextContent('120');
  });

  it('writes a tempo change to the global effect, not the metronome', () => {
    // The delay reads the same value, so a tempo that lived on the metronome
    // would leave a synced repeat pointing at a different beat.
    const { onParameterChange } = renderPanel();

    fireEvent.change(screen.getByLabelText('Atur tempo'), { target: { value: '96' } });

    expect(onParameterChange).toHaveBeenCalledWith('global', 'bpm', 96);
  });

  it('starts and stops the click', () => {
    const { onEnabledChange } = renderPanel();

    fireEvent.click(screen.getByRole('button', { name: 'Metronom' }));

    expect(onEnabledChange).toHaveBeenCalledWith('metronome', true);
  });

  it('reports the click as running when it is', () => {
    renderPanel({ metronome: makeMetronome({ enabled: true }) });

    expect(screen.getByRole('button', { name: 'Metronom' })).toHaveAttribute(
      'aria-pressed',
      'true',
    );
  });

  it('draws one beat light per beat in the bar', () => {
    const { container, rerender } = renderPanel();

    expect(container.querySelectorAll('.tempo__beat')).toHaveLength(4);

    const metronome = makeMetronome();
    metronome.parameters[1].value = 3;

    rerender(
      <TempoPanel
        bpm={bpm}
        metronome={metronome}
        onParameterChange={vi.fn()}
        onEnabledChange={vi.fn()}
      />,
    );

    expect(container.querySelectorAll('.tempo__beat')).toHaveLength(3);
  });

  it('locks every control while the engine is unreachable', () => {
    renderPanel({ disabled: true });

    expect(screen.getByRole('button', { name: 'Tap' })).toBeDisabled();
    expect(screen.getByLabelText('Atur tempo')).toBeDisabled();
    expect(screen.getByRole('button', { name: 'Metronom' })).toBeDisabled();
  });

  it('renders nothing before the engine has reported anything', () => {
    const { container } = render(
      <TempoPanel onParameterChange={vi.fn()} onEnabledChange={vi.fn()} />,
    );

    expect(container).toBeEmptyDOMElement();
  });

  describe('tap tempo', () => {
    let now = 0;

    beforeEach(() => {
      now = 1000;
      vi.spyOn(performance, 'now').mockImplementation(() => now);
    });

    afterEach(() => {
      vi.restoreAllMocks();
    });

    function tap(afterMs = 0) {
      now += afterMs;
      fireEvent.click(screen.getByRole('button', { name: 'Tap' }));
    }

    it('says nothing on the first tap', () => {
      // One tap is a point in time, not an interval. Guessing a tempo from it
      // would jump the delay to something arbitrary mid-song.
      const { onParameterChange } = renderPanel();

      tap();

      expect(onParameterChange).not.toHaveBeenCalled();
    });

    it('derives the tempo from the interval between taps', () => {
      const { onParameterChange } = renderPanel();

      tap();
      tap(500); // 500 ms apart is 120 BPM

      expect(onParameterChange).toHaveBeenCalledWith('global', 'bpm', 120);
    });

    it('averages over several taps rather than trusting the last one', () => {
      const { onParameterChange } = renderPanel();

      tap();
      tap(500);
      tap(500);
      tap(500);

      const last = onParameterChange.mock.calls.at(-1);
      expect(last).toEqual(['global', 'bpm', 120]);
    });

    it('starts over after a long pause instead of averaging across it', () => {
      const { onParameterChange } = renderPanel();

      tap();
      tap(500);
      onParameterChange.mockClear();

      // A gap this long is someone starting again, not a 20 BPM tempo.
      tap(5000);
      expect(onParameterChange).not.toHaveBeenCalled();

      tap(300); // 200 BPM
      expect(onParameterChange).toHaveBeenCalledWith('global', 'bpm', 200);
    });

    it('keeps a frantic or glacial tap inside the tempo range', () => {
      const { onParameterChange } = renderPanel();

      tap();
      tap(50); // 1200 BPM if taken at face value

      expect(onParameterChange).toHaveBeenCalledWith('global', 'bpm', bpm.max);
    });
  });
});
