import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { describe, expect, it, vi } from 'vitest';

import { PresetControls } from '../PresetControls';
import type { PresetMetadata } from '../../services/api';

function meta(name: string, overrides: Partial<PresetMetadata> = {}): PresetMetadata {
  return {
    name,
    description: '',
    tags: [],
    favourite: false,
    notes: '',
    savedAt: '2026-07-21T10:00:00+0000',
    ...overrides,
  };
}

// Five, because the search box only appears once the list is long enough to
// need one -- with three presets you can just read them.
const details = [
  meta('Clean Tele', { tags: ['clean', 'funk'], description: 'Buat lagu pertama' }),
  meta('Lead Boost', { tags: ['lead'], favourite: true, notes: 'Naikkan mid' }),
  meta('Crunch Rhythm', { tags: ['crunch'] }),
  meta('Ambient Wash', { tags: ['clean', 'reverb'] }),
  meta('Bass DI', { tags: ['bass'] }),
];

const presets = details.map((entry) => entry.name);

function renderControls(props: Partial<Parameters<typeof PresetControls>[0]> = {}) {
  const handlers = {
    onLoad: vi.fn(),
    onSave: vi.fn(),
    onDelete: vi.fn(),
    onMetadataChange: vi.fn(),
    onExport: vi.fn(),
    onImport: vi.fn(),
  };

  const view = render(
    <PresetControls
      presets={presets}
      details={details}
      selected="Lead Boost"
      busy={false}
      {...handlers}
      {...props}
    />,
  );

  return { ...view, ...handlers };
}

describe('PresetControls', () => {
  it('marks a favourite and shows its tags in the list', () => {
    renderControls();

    expect(screen.getByRole('option', { name: '★ Lead Boost — lead' })).toBeInTheDocument();
    expect(screen.getByRole('option', { name: 'Clean Tele — clean, funk' })).toBeInTheDocument();
  });

  it('toggles favourite without touching anything else', async () => {
    const user = userEvent.setup();
    const { onMetadataChange } = renderControls();

    await user.click(screen.getByRole('button', { name: 'Favorit' }));

    expect(onMetadataChange).toHaveBeenCalledWith('Lead Boost', { favourite: false });
  });

  it('writes tags as a list, dropping the blanks', async () => {
    const user = userEvent.setup();
    const { onMetadataChange } = renderControls();

    const field = screen.getByLabelText('Tag preset');
    await user.clear(field);
    await user.type(field, 'lead, , solo,  boost ');
    await user.tab();

    expect(onMetadataChange).toHaveBeenCalledWith('Lead Boost', {
      tags: ['lead', 'solo', 'boost'],
    });
  });

  it('keeps notes local until the field is left', async () => {
    // A write per keystroke would be a round trip per character.
    const user = userEvent.setup();
    const { onMetadataChange } = renderControls();

    await user.click(screen.getByRole('button', { name: 'Catatan' }));

    const notes = screen.getByLabelText('Catatan preset');
    expect(notes).toHaveValue('Naikkan mid');

    await user.type(notes, ' sedikit');
    expect(onMetadataChange).not.toHaveBeenCalled();

    await user.tab();
    expect(onMetadataChange).toHaveBeenCalledWith('Lead Boost', {
      notes: 'Naikkan mid sedikit',
    });
  });

  it('narrows the list by tag', async () => {
    // Tags are only worth typing if they are what you can search by.
    const user = userEvent.setup();
    renderControls();

    await user.type(screen.getByLabelText('Cari preset'), 'funk');

    expect(screen.getByRole('option', { name: /Clean Tele/ })).toBeInTheDocument();
    expect(screen.queryByRole('option', { name: /Lead Boost/ })).not.toBeInTheDocument();
  });

  it('narrows the list by description', async () => {
    const user = userEvent.setup();
    renderControls();

    await user.type(screen.getByLabelText('Cari preset'), 'lagu pertama');

    expect(screen.getByRole('option', { name: /Clean Tele/ })).toBeInTheDocument();
    expect(screen.queryByRole('option', { name: /Crunch Rhythm/ })).not.toBeInTheDocument();
  });

  it('says so when nothing matches', async () => {
    const user = userEvent.setup();
    renderControls();

    await user.type(screen.getByLabelText('Cari preset'), 'zzzz');

    expect(screen.getByRole('option', { name: 'Tidak ada yang cocok' })).toBeInTheDocument();
  });

  it('exports the selected preset', async () => {
    const user = userEvent.setup();
    const { onExport } = renderControls();

    await user.click(screen.getByRole('button', { name: 'Ekspor' }));

    expect(onExport).toHaveBeenCalledWith('Lead Boost');
  });

  it('imports a file under the name it came with', async () => {
    const user = userEvent.setup();
    const { onImport } = renderControls();

    const file = new File(['{"state":{}}'], 'My Tone.milodikfx.json', {
      type: 'application/json',
    });

    await user.upload(screen.getByLabelText('Berkas preset'), file);

    // The .milodikfx.json suffix is stripped, not just .json -- otherwise every
    // imported preset would be called "Something.milodikfx".
    await vi.waitFor(() => expect(onImport).toHaveBeenCalledWith('My Tone', '{"state":{}}'));
  });

  it('works without the metadata handlers at all', () => {
    // A build that has no metadata support must still be able to save and load.
    render(
      <PresetControls
        presets={presets}
        selected="Lead Boost"
        busy={false}
        onLoad={vi.fn()}
        onSave={vi.fn()}
        onDelete={vi.fn()}
      />,
    );

    expect(screen.getByRole('button', { name: 'Simpan' })).toBeInTheDocument();
    expect(screen.queryByRole('button', { name: 'Favorit' })).not.toBeInTheDocument();
    expect(screen.queryByRole('button', { name: 'Ekspor' })).not.toBeInTheDocument();
  });

  it('still saves and deletes', async () => {
    const user = userEvent.setup();
    const { onSave, onDelete } = renderControls();

    await user.type(screen.getByLabelText('Nama preset'), 'Baru');
    await user.click(screen.getByRole('button', { name: 'Simpan' }));
    expect(onSave).toHaveBeenCalledWith('Baru');

    await user.click(screen.getByRole('button', { name: 'Hapus' }));
    expect(onDelete).toHaveBeenCalledWith('Lead Boost');
  });
});
