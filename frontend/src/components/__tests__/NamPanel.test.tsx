import { render, screen, waitFor } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { NamPanel } from '../NamPanel';
import type { NamLibraryResponse } from '../../services/api';

const { getNamLibrary, revealNamFolder, importNam } = vi.hoisted(() => ({
  getNamLibrary: vi.fn(),
  revealNamFolder: vi.fn(),
  importNam: vi.fn(),
}));

vi.mock('../../services/api', async (importOriginal) => ({
  ...(await importOriginal<typeof import('../../services/api')>()),
  getNamLibrary,
  revealNamFolder,
  importNam,
}));

function makeLibrary(overrides: Partial<NamLibraryResponse> = {}): NamLibraryResponse {
  return {
    models: ['Marshall JVM', 'Fender Twin'],
    directory: 'C:\\Users\\me\\Documents\\MilodikFX\\NamModels',
    available: true,
    unavailableReason: '',
    ...overrides,
  };
}

describe('NamPanel', () => {
  beforeEach(() => {
    getNamLibrary.mockResolvedValue(makeLibrary());
    revealNamFolder.mockResolvedValue({ revealed: 'C:\\...' });
    importNam.mockResolvedValue({ name: 'New Amp', models: ['New Amp'] });
  });

  afterEach(() => {
    vi.clearAllMocks();
  });

  async function renderPanel(props: Partial<Parameters<typeof NamPanel>[0]> = {}) {
    const onLibraryChanged = vi.fn();
    const view = render(<NamPanel onLibraryChanged={onLibraryChanged} {...props} />);
    await waitFor(() => expect(getNamLibrary).toHaveBeenCalled());
    return { ...view, onLibraryChanged };
  }

  it('shows how many models are on disk', async () => {
    await renderPanel();

    expect(await screen.findByText('2')).toBeInTheDocument();
  });

  it('opens the models folder', async () => {
    const user = userEvent.setup();
    await renderPanel();

    await user.click(screen.getByRole('button', { name: 'Buka folder' }));

    expect(revealNamFolder).toHaveBeenCalled();
  });

  it('imports a .nam file as base64 without the data-URL prefix', async () => {
    const user = userEvent.setup();
    const { onLibraryChanged } = await renderPanel();

    // "hello" base64-encoded is aGVsbG8=; FileReader prefixes a data: URL that
    // must be stripped before the engine sees it.
    const file = new File(['hello'], 'My Amp.nam', { type: 'application/octet-stream' });

    await user.upload(screen.getByLabelText('Berkas model NAM'), file);

    await waitFor(() => expect(importNam).toHaveBeenCalled());
    const [name, base64] = importNam.mock.calls[0];
    expect(name).toBe('My Amp');
    expect(base64).toBe('aGVsbG8=');
    expect(base64).not.toContain(',');

    await waitFor(() => expect(onLibraryChanged).toHaveBeenCalled());
  });

  it('explains why loading is disabled when the engine cannot run models', async () => {
    // A build without AVX2: the reason must be shown rather than an import that
    // silently fails.
    getNamLibrary.mockResolvedValue(
      makeLibrary({ available: false, unavailableReason: 'CPU ini tidak mendukung AVX2', models: [] }),
    );

    await renderPanel();

    expect(await screen.findByText('CPU ini tidak mendukung AVX2')).toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Impor model' })).toBeDisabled();
  });

  it('still lets you open the folder even when models cannot run', async () => {
    getNamLibrary.mockResolvedValue(makeLibrary({ available: false, unavailableReason: 'x' }));

    await renderPanel();

    expect(screen.getByRole('button', { name: 'Buka folder' })).not.toBeDisabled();
  });

  it('locks its controls while the engine is unreachable', async () => {
    await renderPanel({ disabled: true });

    expect(screen.getByRole('button', { name: 'Buka folder' })).toBeDisabled();
    expect(screen.getByRole('button', { name: 'Impor model' })).toBeDisabled();
  });
});
