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
    const card = document.querySelector('.border-green-500');
    expect(card).toBeInTheDocument();
  });

  it('shows reduced opacity when disabled', () => {
    render(
      <EffectCard
        type="GAIN"
        title="Gain"
        enabled={false}
        parameters={mockParams}
        onToggle={vi.fn()}
        onParameterChange={vi.fn()}
      />
    );
    const card = document.querySelector('.opacity-50');
    expect(card).toBeInTheDocument();
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
    
    const toggleButton = screen.getByRole('button', { name: '' }); // Toggle switch
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
    expect(screen.getByText('Level')).toBeInTheDocument();
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
