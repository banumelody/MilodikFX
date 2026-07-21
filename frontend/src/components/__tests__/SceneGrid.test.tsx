import { render, screen, waitFor } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { SceneGrid } from '../SceneGrid';
import type { EffectDescriptor, ScenesState } from '../../services/api';

const { getScenes, recallScene, captureScene, renameScene, setSceneEffect } = vi.hoisted(() => ({
  getScenes: vi.fn(),
  recallScene: vi.fn(),
  captureScene: vi.fn(),
  renameScene: vi.fn(),
  setSceneEffect: vi.fn(),
}));

vi.mock('../../services/api', async (importOriginal) => ({
  ...(await importOriginal<typeof import('../../services/api')>()),
  getScenes,
  recallScene,
  captureScene,
  renameScene,
  setSceneEffect,
}));

function makeEffect(id: string, label: string, toggleable = true): EffectDescriptor {
  return { id, label, description: '', enabled: true, toggleable, parameters: [] };
}

const effects = [
  makeEffect('overdrive', 'Overdrive'),
  makeEffect('delay', 'Delay'),
  makeEffect('master', 'Master', false),
];

function makeState(overrides: Partial<ScenesState> = {}): ScenesState {
  return {
    active: 0,
    scenes: [
      { index: 0, name: 'Clean', enabled: { overdrive: false, delay: false }, populated: true },
      { index: 1, name: 'Crunch', enabled: { overdrive: true, delay: false }, populated: true },
      { index: 2, name: 'Lead', enabled: { overdrive: true, delay: true }, populated: true },
      { index: 3, name: 'Solo', enabled: {}, populated: false },
    ],
    ...overrides,
  };
}

describe('SceneGrid', () => {
  beforeEach(() => {
    getScenes.mockResolvedValue(makeState());
    recallScene.mockResolvedValue(makeState({ active: 1 }));
    captureScene.mockResolvedValue(makeState());
    renameScene.mockResolvedValue(makeState());
    setSceneEffect.mockResolvedValue(makeState({ active: -1 }));
  });

  afterEach(() => {
    vi.clearAllMocks();
  });

  async function renderGrid(props: Partial<Parameters<typeof SceneGrid>[0]> = {}) {
    const onRecalled = vi.fn();
    const view = render(
      <SceneGrid effects={effects} onRecalled={onRecalled} {...props} />,
    );
    await waitFor(() => expect(getScenes).toHaveBeenCalled());
    return { ...view, onRecalled };
  }

  it('shows one row per scene', async () => {
    await renderGrid();

    for (const name of ['Clean', 'Crunch', 'Lead', 'Solo']) {
      expect(await screen.findByText(name)).toBeInTheDocument();
    }
  });

  it('leaves out effects a scene cannot switch', async () => {
    // Master is always in the path, so a column for it could never do anything.
    const { container } = await renderGrid();

    await waitFor(() => expect(container.querySelectorAll('.scenes__col')).toHaveLength(2));
    expect(screen.queryByLabelText(/Master (menyala|mati)/)).not.toBeInTheDocument();
  });

  it('marks each cell with whether that effect is on in that scene', async () => {
    await renderGrid();

    expect(await screen.findByLabelText('Clean: Overdrive mati')).toHaveAttribute(
      'aria-pressed',
      'false',
    );
    expect(screen.getByLabelText('Lead: Delay menyala')).toHaveAttribute('aria-pressed', 'true');
  });

  it('recalls a scene and tells the caller so the rack can catch up', async () => {
    const user = userEvent.setup();
    const { onRecalled } = await renderGrid();

    await user.click(await screen.findByRole('rowheader', { name: /Crunch/ }));

    expect(recallScene).toHaveBeenCalledWith(1);
    await waitFor(() => expect(onRecalled).toHaveBeenCalled());
  });

  it('highlights the scene that is playing', async () => {
    await renderGrid();

    expect(await screen.findByRole('rowheader', { name: /Clean/ })).toHaveAttribute(
      'aria-pressed',
      'true',
    );
    expect(screen.getByRole('rowheader', { name: /Crunch/ })).toHaveAttribute(
      'aria-pressed',
      'false',
    );
  });

  it('says when the chain matches no scene at all', async () => {
    // After a knob or switch has been moved by hand, highlighting a slot would
    // claim you are hearing something you are not.
    getScenes.mockResolvedValue(makeState({ active: -1 }));

    await renderGrid();

    expect(await screen.findByText('Diubah manual')).toBeInTheDocument();
  });

  it('edits a scene cell without recalling it', async () => {
    const user = userEvent.setup();
    const { onRecalled } = await renderGrid();

    await user.click(await screen.findByLabelText('Crunch: Delay mati'));

    expect(setSceneEffect).toHaveBeenCalledWith(1, 'delay', true);
    expect(recallScene).not.toHaveBeenCalled();
    expect(onRecalled).not.toHaveBeenCalled();
  });

  it('stores the chain as it stands into a slot', async () => {
    const user = userEvent.setup();
    await renderGrid();

    await user.click(
      await screen.findByRole('button', { name: 'Rekam kondisi sekarang ke Solo' }),
    );

    expect(captureScene).toHaveBeenCalledWith(3);
  });

  it('renames a scene on double click', async () => {
    const user = userEvent.setup();
    await renderGrid();

    await user.dblClick(await screen.findByRole('rowheader', { name: /Clean/ }));

    const field = screen.getByLabelText('Nama scene 1');
    await user.clear(field);
    await user.type(field, 'Rhythm');
    await user.tab();

    expect(renameScene).toHaveBeenCalledWith(0, 'Rhythm');
  });

  it('does not write a rename that changed nothing', async () => {
    const user = userEvent.setup();
    await renderGrid();

    await user.dblClick(await screen.findByRole('rowheader', { name: /Clean/ }));
    await user.tab();

    expect(renameScene).not.toHaveBeenCalled();
  });

  it('locks everything while the engine is unreachable', async () => {
    await renderGrid({ disabled: true });

    expect(await screen.findByRole('rowheader', { name: /Clean/ })).toBeDisabled();
    expect(screen.getByLabelText('Clean: Overdrive mati')).toBeDisabled();
  });

  it('renders nothing when no effect can be switched', async () => {
    const { container } = render(
      <SceneGrid effects={[makeEffect('master', 'Master', false)]} onRecalled={vi.fn()} />,
    );

    expect(container).toBeEmptyDOMElement();
  });

  it('surfaces what the engine refused', async () => {
    recallScene.mockRejectedValue(new Error('That scene is empty'));

    const user = userEvent.setup();
    await renderGrid();

    await user.click(await screen.findByRole('rowheader', { name: /Solo/ }));

    expect(await screen.findByText('That scene is empty')).toBeInTheDocument();
  });
});
