import { describe, it, expect, vi } from 'vitest';
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { EffectCard } from '../EffectCard';

describe('EffectCard', () => {
  const mockParams = [
    { id: 'level', name: 'Level', value: 0, min: -24, max: 24, unit: 'dB' },
  ];

  it('renders effect title and type', () => {
    render(
      <EffectCard
        type="GAIN"
        title="Gain"
        enabled={true}
        parameters={mockParams}
        onToggle={vi.fn()}
        onParameterChange={vi.fn()}
      />
    );
    expect(screen.getByText('Gain')).toBeInTheDocument();
    expect(screen.getByText('GAIN')).toBeInTheDocument();
  });

  it('renders with correct color based on effect type', () => {
    const { container } = render(
      <EffectCard
        type="GAIN"
        title="Gain"
        enabled={true}
        parameters={mockParams}
        onToggle={vi.fn()}
        onParameterChange={vi.fn()}
      />
    );
    const card = container.querySelector('div[class*="rounded-lg"][class*="border-2"]');
    expect(card).toBeInTheDocument();
    expect(card?.className).toContain('green');
  });

  it('shows reduced opacity when disabled', () => {
    const { container } = render(
      <EffectCard
        type="GAIN"
        title="Gain"
        enabled={false}
        parameters={mockParams}
        onToggle={vi.fn()}
        onParameterChange={vi.fn()}
      />
    );
    const card = container.querySelector('div[class*="rounded-lg"][class*="border-2"]');
    expect(card?.className).toContain('opacity-50');
  });

  it('calls onToggle when toggle switch is clicked', async () => {
    const onToggle = vi.fn();
    render(
      <EffectCard
        type="GAIN"
        title="Gain"
        enabled={true}
        parameters={mockParams}
        onToggle={onToggle}
        onParameterChange={vi.fn()}
      />
    );
    
    const toggleButton = screen.getAllByRole('button')[0];
    await userEvent.click(toggleButton);
    
    expect(onToggle).toHaveBeenCalled();
  });

  it('renders parameter controls', () => {
    render(
      <EffectCard
        type="GAIN"
        title="Gain"
        enabled={true}
        parameters={mockParams}
        onToggle={vi.fn()}
        onParameterChange={vi.fn()}
      />
    );
    const levelElements = screen.getAllByText('Level');
    expect(levelElements.length).toBeGreaterThan(0);
  });

  it('calls onRemove when remove button is clicked', async () => {
    const onRemove = vi.fn();
    render(
      <EffectCard
        type="GAIN"
        title="Gain"
        enabled={true}
        parameters={mockParams}
        onToggle={vi.fn()}
        onParameterChange={vi.fn()}
        onRemove={onRemove}
      />
    );
    
    const removeButton = screen.getByTitle('Remove effect');
    await userEvent.click(removeButton);
    
    expect(onRemove).toHaveBeenCalled();
  });

  it('does not show remove button when onRemove is not provided', () => {
    render(
      <EffectCard
        type="GAIN"
        title="Gain"
        enabled={true}
        parameters={mockParams}
        onToggle={vi.fn()}
        onParameterChange={vi.fn()}
      />
    );
    
    expect(screen.queryByTitle('Remove effect')).not.toBeInTheDocument();
  });
});
