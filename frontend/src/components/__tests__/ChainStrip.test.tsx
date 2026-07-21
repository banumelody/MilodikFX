import { fireEvent, render, screen } from '@testing-library/react';
import { describe, expect, it, vi } from 'vitest';

import { ChainStrip } from '../ChainStrip';
import type { EffectDescriptor } from '../../services/api';

function makeEffect(id: string, label: string, enabled = true, toggleable = true): EffectDescriptor {
  return { id, label, description: '', enabled, toggleable, parameters: [] };
}

const effects: EffectDescriptor[] = [
  makeEffect('global', 'Global', true, false),
  makeEffect('input', 'Input', true, false),
  makeEffect('noiseGate', 'Noise Gate'),
  makeEffect('overdrive', 'Overdrive', false),
  makeEffect('master', 'Master', true, false),
];

function renderStrip(list = effects) {
  const onSelect = vi.fn();
  const onToggle = vi.fn();

  render(<ChainStrip effects={list} onSelect={onSelect} onToggle={onToggle} />);

  return { onSelect, onToggle };
}

describe('ChainStrip', () => {
  it('shows the signal path from input to output', () => {
    renderStrip();

    expect(screen.getByText('IN')).toBeInTheDocument();
    expect(screen.getByText('OUT')).toBeInTheDocument();
  });

  it('leaves out stages that are not points in the signal path', () => {
    // Input routing and the global controls are not things signal passes
    // through, so showing them as blocks would misrepresent the chain.
    renderStrip();

    expect(screen.queryByRole('button', { name: 'Global' })).not.toBeInTheDocument();
    expect(screen.queryByRole('button', { name: 'Input' })).not.toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Noise Gate' })).toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Master' })).toBeInTheDocument();
  });

  it('keeps the blocks in chain order', () => {
    renderStrip();

    const names = screen.getAllByRole('button').map((button) => button.textContent);
    expect(names).toEqual(['Noise Gate', 'Overdrive', 'Master']);
  });

  it('reflects whether each stage is on', () => {
    renderStrip();

    expect(screen.getByRole('button', { name: 'Noise Gate' })).toHaveAttribute(
      'aria-pressed',
      'true',
    );
    expect(screen.getByRole('button', { name: 'Overdrive' })).toHaveAttribute(
      'aria-pressed',
      'false',
    );
  });

  it('navigates to a stage on click', () => {
    const { onSelect, onToggle } = renderStrip();

    fireEvent.click(screen.getByRole('button', { name: 'Overdrive' }));

    expect(onSelect).toHaveBeenCalledWith('overdrive');
    expect(onToggle).not.toHaveBeenCalled();
  });

  it('toggles a stage on right-click', () => {
    const { onToggle } = renderStrip();

    fireEvent.contextMenu(screen.getByRole('button', { name: 'Overdrive' }));

    expect(onToggle).toHaveBeenCalledWith('overdrive', true);
  });

  it('refuses to toggle a stage that is always in the path', () => {
    const { onToggle } = renderStrip();

    fireEvent.contextMenu(screen.getByRole('button', { name: 'Master' }));

    expect(onToggle).not.toHaveBeenCalled();
  });

  it('renders nothing when there is no chain yet', () => {
    const { container } = render(
      <ChainStrip effects={[]} onSelect={vi.fn()} onToggle={vi.fn()} />,
    );

    expect(container).toBeEmptyDOMElement();
  });
});
