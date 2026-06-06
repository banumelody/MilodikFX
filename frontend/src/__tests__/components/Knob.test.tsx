import { describe, it, expect } from 'vitest';
import { render, screen } from '@testing-library/react';
import { Knob } from '../../components/Knob';

describe('Knob Component', () => {
  it('renders knob with label', () => {
    const { container } = render(
      <Knob
        value={5}
        min={0}
        max={10}
        onChange={() => {}}
        label="Test Knob"
      />
    );

    expect(screen.getByText('Test Knob')).toBeInTheDocument();
    expect(container.querySelector('canvas')).toBeInTheDocument();
  });

  it('displays current value', () => {
    render(
      <Knob
        value={5.25}
        min={0}
        max={10}
        onChange={() => {}}
      />
    );

    expect(screen.getByText('5.25')).toBeInTheDocument();
  });

  it('respects min/max boundaries', () => {
    render(
      <Knob
        value={10}
        min={0}
        max={10}
        onChange={() => {}}
      />
    );

    expect(screen.getByText('10.00')).toBeInTheDocument();
  });

  it('has canvas element', () => {
    const { container } = render(
      <Knob
        value={5}
        min={0}
        max={10}
        onChange={() => {}}
        disabled={false}
      />
    );

    const canvas = container.querySelector('canvas');
    expect(canvas).toBeTruthy();
    expect(canvas).toHaveAttribute('width');
    expect(canvas).toHaveAttribute('height');
  });
});
