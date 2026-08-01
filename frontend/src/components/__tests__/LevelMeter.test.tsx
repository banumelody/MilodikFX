import { render, screen } from '@testing-library/react';
import { describe, expect, it } from 'vitest';

import { LevelMeter, ReductionMeter } from '../LevelMeter';

describe('LevelMeter', () => {
  it('shows the level it was given', () => {
    render(<LevelMeter label="Input" db={-18.3} />);

    expect(screen.getByText('-18.3 dB')).toBeInTheDocument();
    expect(screen.getByRole('meter', { name: 'Input' })).toHaveAttribute(
      'aria-valuenow',
      '-18.3',
    );
  });

  it('shows a dash rather than a noise-floor number in silence', () => {
    render(<LevelMeter label="Input" db={-98} />);

    expect(screen.getByText('--')).toBeInTheDocument();
  });

  it('pins the bar to the bottom of its scale rather than going negative', () => {
    const { container } = render(<LevelMeter label="Input" db={-200} />);

    const fill = container.querySelector<HTMLElement>('.meter__fill');
    expect(fill?.style.width).toBe('0%');
  });

  it('pins the bar to the top rather than overflowing', () => {
    const { container } = render(<LevelMeter label="Input" db={40} />);

    const fill = container.querySelector<HTMLElement>('.meter__fill');
    expect(fill?.style.width).toBe('100%');
  });

  describe('clipping upstream of the trim', () => {
    it('reports CLIP even when the trimmed level looks healthy', () => {
      // The case the whole sourceDb prop exists for: the interface is clipping,
      // the trim pulled the reading down to a comfortable -12, and the bar
      // would otherwise say everything is fine.
      const { container } = render(<LevelMeter label="Input" db={-12} sourceDb={-0.1} />);

      expect(screen.getByText('CLIP')).toBeInTheDocument();
      expect(screen.queryByText('-12.0 dB')).not.toBeInTheDocument();
      expect(container.querySelector('.meter__fill--hot')).toBeInTheDocument();
    });

    it('stays quiet when the source is not clipping', () => {
      const { container } = render(<LevelMeter label="Input" db={-12} sourceDb={-24} />);

      expect(screen.getByText('-12.0 dB')).toBeInTheDocument();
      expect(screen.queryByText('CLIP')).not.toBeInTheDocument();
      expect(container.querySelector('.meter__fill--hot')).not.toBeInTheDocument();
    });

    it('still flags a hot level when no source figure is supplied', () => {
      const { container } = render(<LevelMeter label="Output" db={-0.2} />);

      expect(container.querySelector('.meter__fill--hot')).toBeInTheDocument();
      expect(screen.getByText('-0.2 dB')).toBeInTheDocument();
    });
  });
});

describe('ReductionMeter', () => {
  it('shows nothing while the compressor is not working', () => {
    render(<ReductionMeter label="Comp" db={0} />);

    expect(screen.getByText('--')).toBeInTheDocument();
  });

  it('shows gain reduction as a positive amount of cut', () => {
    render(<ReductionMeter label="Comp" db={-6.4} />);

    expect(screen.getByText('-6.4 dB')).toBeInTheDocument();
  });

  it('does not run past the end of its scale', () => {
    const { container } = render(<ReductionMeter label="Comp" db={-999} maxDb={24} />);

    const fill = container.querySelector<HTMLElement>('.meter__fill--reduction');
    expect(fill?.style.width).toBe('100%');
  });
});

describe('LevelMeter per channel', () => {
  it('stays a single bar when no channels are given', () => {
    const { container } = render(<LevelMeter label="Output" db={-12} />);

    expect(container.querySelector('.meter__track--split')).not.toBeInTheDocument();
    expect(container.querySelectorAll('.meter__lane')).toHaveLength(0);
  });

  it('splits into two lanes when the sides differ', () => {
    const { container } = render(
      <LevelMeter label="Output" db={-8.4} dbL={-8.4} dbR={-14.1} />,
    );

    const lanes = container.querySelectorAll('.meter__lane');
    expect(lanes).toHaveLength(2);
    expect(lanes[0].textContent).toBe('L');
    expect(lanes[1].textContent).toBe('R');
  });

  it('reads out both figures', () => {
    render(<LevelMeter label="Output" db={-8.4} dbL={-8.4} dbR={-14.1} />);

    expect(screen.getByText('-8.4 / -14.1 dB')).toBeInTheDocument();
  });

  it('keeps the accessible value on the combined figure', () => {
    // The meter role reports one number, and the louder side is the one that
    // answers "am I about to clip".
    render(<LevelMeter label="Output" db={-3} dbL={-3} dbR={-40} />);

    const meter = screen.getByRole('meter', { name: 'Output' });
    expect(meter).toHaveAttribute('aria-valuenow', '-3');
  });

  it('still says CLIP over the numbers', () => {
    // A trimmed-down reading looks healthy while the converter clips, and no
    // digital trim can undo that -- so the warning outranks the per-channel
    // detail rather than being crowded out by it.
    render(<LevelMeter label="Input" db={-20} dbL={-20} dbR={-26} sourceDb={0.2} />);

    expect(screen.getByText('CLIP')).toBeInTheDocument();
  });

  it('draws each lane to its own width', () => {
    const { container } = render(
      // -60 is the floor and 6 the ceiling, so -60 is empty and 6 is full.
      <LevelMeter label="Output" db={6} dbL={6} dbR={-60} />,
    );

    const fills = container.querySelectorAll<HTMLElement>('.meter__lane .meter__fill');
    expect(fills[0].style.width).toBe('100%');
    expect(fills[1].style.width).toBe('0%');
  });
});
