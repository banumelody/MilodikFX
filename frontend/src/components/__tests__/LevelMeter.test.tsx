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
