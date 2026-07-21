import { render } from '@testing-library/react';
import { describe, expect, it } from 'vitest';

import { ToneCurve } from '../ToneCurve';
import type { EffectDescriptor } from '../../services/api';

function makeStage(id: string, gains: Record<string, number>, enabled = true): EffectDescriptor {
  return {
    id,
    label: id === 'eq' ? 'EQ' : 'Contour',
    description: '',
    enabled,
    toggleable: true,
    parameters: Object.entries(gains).map(([parameterId, value]) => ({
      id: parameterId,
      label: parameterId,
      unit: 'dB',
      min: -12,
      max: 12,
      step: 0.1,
      default: 0,
      type: 'float' as const,
      value,
    })),
  };
}

/** The y of the highest point on the path; SVG y grows downwards. */
function highestPoint(path: string) {
  const ys = [...path.matchAll(/[ML]([-\d.]+),([-\d.]+)/g)].map((match) => Number(match[2]));
  return Math.min(...ys);
}

describe('ToneCurve', () => {
  it('draws a curve for a tone stage', () => {
    const { container } = render(<ToneCurve effect={makeStage('eq', { midDb: 6 })} />);

    const path = container.querySelector('path.tone-curve__line');
    expect(path).not.toBeNull();
    expect(path?.getAttribute('d')?.length ?? 0).toBeGreaterThan(50);
  });

  it('draws nothing for a stage that has no tone bands', () => {
    const { container } = render(
      <ToneCurve effect={makeStage('overdrive', { drivePct: 50 })} />,
    );

    expect(container).toBeEmptyDOMElement();
  });

  it('rises where the band is boosted and falls where it is cut', () => {
    const boost = render(<ToneCurve effect={makeStage('eq', { midDb: 12 })} />);
    const flat = render(<ToneCurve effect={makeStage('eq', { midDb: 0 })} />);
    const cut = render(<ToneCurve effect={makeStage('eq', { midDb: -12 })} />);

    const y = (view: ReturnType<typeof render>) =>
      highestPoint(view.container.querySelector('path.tone-curve__line')!.getAttribute('d')!);

    // Higher on screen means a smaller y.
    expect(y(boost)).toBeLessThan(y(flat));
    expect(y(cut)).toBeGreaterThanOrEqual(y(flat));
  });

  it('says in its label when the stage is doing nothing', () => {
    // A flat line and a barely-curved one look the same at this size, so the
    // accessible name has to carry the difference.
    const { container } = render(<ToneCurve effect={makeStage('eq', { midDb: 0 })} />);

    expect(container.querySelector('svg')).toHaveAttribute(
      'aria-label',
      'Kurva respons EQ, rata',
    );
  });

  it('dims itself when the stage is switched off', () => {
    const { container } = render(
      <ToneCurve effect={makeStage('eq', { midDb: 6 }, false)} />,
    );

    expect(container.querySelector('svg')).toHaveClass('tone-curve--off');
  });

  it('stays within its box however far the knobs are pushed', () => {
    // Every band at once can exceed the plot's own range; the line has to be
    // clamped to the viewBox rather than drawn off the top of it.
    const { container } = render(
      <ToneCurve effect={makeStage('toneStack', { bassDb: 12, midDb: 12, trebleDb: 12 })} />,
    );

    const path = container.querySelector('path.tone-curve__line')!.getAttribute('d')!;
    const ys = [...path.matchAll(/[ML]([-\d.]+),([-\d.]+)/g)].map((match) => Number(match[2]));

    for (const y of ys) {
      expect(y).toBeGreaterThanOrEqual(0);
      expect(y).toBeLessThanOrEqual(90);
    }
  });

  it('takes the sample rate it is given', () => {
    // At 8 kHz the 7 kHz shelf is past Nyquist, so the two curves must differ.
    const fast = render(<ToneCurve effect={makeStage('eq', { trebleDb: 12 })} sampleRate={48000} />);
    const slow = render(<ToneCurve effect={makeStage('eq', { trebleDb: 12 })} sampleRate={8000} />);

    const d = (view: ReturnType<typeof render>) =>
      view.container.querySelector('path.tone-curve__line')!.getAttribute('d');

    expect(d(fast)).not.toEqual(d(slow));
  });
});
