import { describe, it, expect } from 'vitest';
import { render, screen } from '@testing-library/react';
import { Meter } from '../../components/Meter';

describe('Meter Component', () => {
  it('renders meter with label', () => {
    render(<Meter level={0.5} label="Test Meter" />);

    expect(screen.getByText('Test Meter')).toBeInTheDocument();
  });

  it('displays level percentage', () => {
    render(<Meter level={0.75} />);

    expect(screen.getByText('75%')).toBeInTheDocument();
  });

  it('renders stereo meters when stereo prop is true', () => {
    const { container } = render(
      <Meter
        level={0.5}
        stereo={true}
        rightLevel={0.6}
        label="Stereo Meter"
      />
    );

    const meterBars = container.querySelectorAll('div[style*="height"]');
    expect(meterBars.length).toBeGreaterThanOrEqual(2);
  });

  it('shows 0% for zero level', () => {
    render(<Meter level={0} />);

    expect(screen.getByText('0%')).toBeInTheDocument();
  });

  it('shows 100% for full level', () => {
    render(<Meter level={1} />);

    expect(screen.getByText('100%')).toBeInTheDocument();
  });
});
