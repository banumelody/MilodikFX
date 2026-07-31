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

describe('ChainStrip router', () => {
  const stage = (
    id: string,
    label: string,
    extra: Partial<EffectDescriptor> = {},
  ): EffectDescriptor => ({
    id,
    label,
    description: label,
    enabled: true,
    toggleable: true,
    parameters: [],
    ...extra,
  });

  const withSection = [
    stage('noiseGate', 'Noise Gate'),
    stage('split', 'Split'),
    stage('overdrive', 'Overdrive'),
    stage('reverb', 'Reverb'),
    stage('mixer', 'Mixer'),
    stage('master', 'Master'),
  ];

  it('draws two bus lines between the Splitter and the Mixer', () => {
    const { container } = render(
      <ChainStrip
        effects={withSection}
        onSelect={vi.fn()}
        onToggle={vi.fn()}
        busB={new Set(['reverb'])}
      />,
    );

    const rails = container.querySelectorAll('.chain__rail');
    expect(rails).toHaveLength(2);

    // Apple's convention, and it earns itself: path A runs straight through and
    // B is the detour drawn over the top, so B is the upper line.
    expect(rails[0].className).toContain('chain__rail--b');
    expect(rails[1].className).toContain('chain__rail--a');

    expect(rails[0].textContent).toContain('Reverb');
    expect(rails[1].textContent).toContain('Overdrive');
  });

  it('keeps blocks outside the section on the single line', () => {
    const { container } = render(
      <ChainStrip effects={withSection} onSelect={vi.fn()} onToggle={vi.fn()} busB={new Set()} />,
    );

    const fork = container.querySelector('.chain__fork');
    expect(fork?.textContent).not.toContain('Noise Gate');
    expect(fork?.textContent).not.toContain('Master');
  });

  it('says a bus is empty rather than drawing nothing', () => {
    const { container } = render(
      <ChainStrip effects={withSection} onSelect={vi.fn()} onToggle={vi.fn()} busB={new Set()} />,
    );

    // Everything is on A, so B carries the signal untouched. A blank line would
    // read as broken; it is a wire.
    const rails = container.querySelectorAll('.chain__rail');
    expect(rails[0].textContent).toContain('lewat begitu saja');
  });

  it('stays a single row when there is no Splitter', () => {
    const { container } = render(
      <ChainStrip
        effects={[stage('overdrive', 'Overdrive'), stage('reverb', 'Reverb')]}
        onSelect={vi.fn()}
        onToggle={vi.fn()}
      />,
    );

    expect(container.querySelector('.chain__fork')).not.toBeInTheDocument();
  });

  it('leaves blocks that are off the board out of the path', () => {
    render(
      <ChainStrip
        effects={[
          stage('overdrive', 'Overdrive'),
          stage('reverb', 'Reverb', { placed: false }),
        ]}
        onSelect={vi.fn()}
        onToggle={vi.fn()}
      />,
    );

    expect(screen.getByRole('button', { name: 'Overdrive' })).toBeInTheDocument();
    expect(screen.queryByRole('button', { name: 'Reverb' })).not.toBeInTheDocument();
  });

  it('shows an empty board as a straight wire, but a loading one as nothing', () => {
    const { container: emptyBoard } = render(
      <ChainStrip
        effects={[stage('overdrive', 'Overdrive', { placed: false })]}
        onSelect={vi.fn()}
        onToggle={vi.fn()}
      />,
    );

    expect(emptyBoard.textContent).toContain('kabel lurus');

    const { container: loading } = render(
      <ChainStrip effects={[]} onSelect={vi.fn()} onToggle={vi.fn()} />,
    );

    // Not yet loaded is not the same as nothing placed: drawing a wire before
    // /api/effects lands would claim the board is empty when it is unknown.
    expect(loading).toBeEmptyDOMElement();
  });
});
