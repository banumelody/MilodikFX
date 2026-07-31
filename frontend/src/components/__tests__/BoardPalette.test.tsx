import { fireEvent, render, screen } from '@testing-library/react';
import { describe, expect, it, vi } from 'vitest';

import { BoardPalette } from '../BoardPalette';
import type { EffectDescriptor } from '../../services/api';

function stage(id: string, label: string, placed: boolean): EffectDescriptor {
  return {
    id,
    label,
    description: `${label} description`,
    enabled: true,
    toggleable: true,
    parameters: [],
    placed,
  };
}

const stages = [
  stage('overdrive', 'Overdrive', false),
  stage('delay', 'Delay', false),
  stage('reverb', 'Reverb', true),
  stage('split', 'Split', false),
  stage('mixer', 'Mixer', false),
];

function renderPalette(overrides: Partial<React.ComponentProps<typeof BoardPalette>> = {}) {
  const paletteProps = vi.fn(() => ({}));

  render(
    <BoardPalette stages={stages} paletteProps={paletteProps} {...overrides} />,
  );

  return { paletteProps };
}

describe('BoardPalette', () => {
  it('offers only the blocks that are off the board', () => {
    renderPalette();

    expect(screen.getByRole('button', { name: /Overdrive/ })).toBeInTheDocument();
    expect(screen.getByRole('button', { name: /Delay/ })).toBeInTheDocument();

    // Reverb is already in the rack, so offering it again would let someone
    // build a board with two of a block the engine has only one of.
    expect(screen.queryByRole('button', { name: /Reverb/ })).not.toBeInTheDocument();
  });

  it('never offers the Mixer on its own', () => {
    renderPalette();

    // Apple's rule: the Mixer arrives with the Splitter and leaves with it. On
    // its own it would be a mix point with nothing to mix.
    expect(screen.queryByRole('button', { name: /Mixer/ })).not.toBeInTheDocument();
    expect(screen.getByRole('button', { name: /Split/ })).toBeInTheDocument();
  });

  it('says what the Splitter will do before it is dragged', () => {
    renderPalette();

    // A plain substring check rather than an asymmetric matcher inside
    // toHaveAttribute: that combination compares against the matcher's
    // *description* and passes on strings it should reject.
    expect(screen.getByRole('button', { name: /Split/ }).getAttribute('title')).toContain(
      'Mixer menyusul sendiri',
    );
  });

  it('groups the blocks by what they do to the signal', () => {
    renderPalette();

    // Grouped rather than alphabetical: it is the order anyone building a rig
    // thinks in, and the palette is read while deciding, not while searching.
    const titles = screen.getAllByText(/^(Utilitas|Dinamika|Drive|Nada|Amp|Ruang)$/);
    expect(titles.map((node) => node.textContent)).toEqual([
      'Utilitas',
      'Drive',
      'Ruang',
    ]);
  });

  it('says so when everything is already placed', () => {
    render(
      <BoardPalette
        stages={stages.map((s) => ({ ...s, placed: true }))}
        paletteProps={vi.fn(() => ({}))}
      />,
    );

    expect(screen.getByText('Semua blok sudah ada di board.')).toBeInTheDocument();
  });

  it('wires each chip to the drag hook', () => {
    const onPointerDown = vi.fn();
    const paletteProps = vi.fn(() => ({ onPointerDown }));

    render(<BoardPalette stages={stages} paletteProps={paletteProps} />);

    fireEvent.pointerDown(screen.getByRole('button', { name: /Overdrive/ }));
    expect(onPointerDown).toHaveBeenCalled();
    expect(paletteProps).toHaveBeenCalledWith('overdrive');
  });

  it('disables every chip while the engine is unreachable', () => {
    renderPalette({ disabled: true });

    expect(screen.getByRole('button', { name: /Overdrive/ })).toBeDisabled();
  });
});
