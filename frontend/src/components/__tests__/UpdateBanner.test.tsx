import { fireEvent, render, screen } from '@testing-library/react';
import { describe, expect, it, vi } from 'vitest';

import { UpdateBanner } from '../UpdateBanner';
import type { UpdateInfo } from '../../services/api';

const available: UpdateInfo = {
  current: '0.14.0',
  latest: 'v0.15.0',
  updateAvailable: true,
  url: 'https://github.com/banumelody/MilodikFX/releases/tag/v0.15.0',
  name: 'MilodikFX 0.15.0',
};

describe('UpdateBanner', () => {
  it('renders nothing when there is no update info', () => {
    const { container } = render(<UpdateBanner info={null} onDismiss={() => {}} />);
    expect(container).toBeEmptyDOMElement();
  });

  it('renders nothing when the running build is already current', () => {
    const { container } = render(
      <UpdateBanner info={{ ...available, updateAvailable: false }} onDismiss={() => {}} />,
    );
    expect(container).toBeEmptyDOMElement();
  });

  it('announces the newer version and links to its release', () => {
    render(<UpdateBanner info={available} onDismiss={() => {}} />);

    expect(screen.getByText(/v0\.15\.0/)).toBeInTheDocument();
    const link = screen.getByRole('link', { name: /lihat rilis/i });
    expect(link).toHaveAttribute('href', available.url);
    expect(link).toHaveAttribute('target', '_blank');
  });

  it('calls back when dismissed', () => {
    const onDismiss = vi.fn();
    render(<UpdateBanner info={available} onDismiss={onDismiss} />);

    fireEvent.click(screen.getByRole('button', { name: /tutup/i }));
    expect(onDismiss).toHaveBeenCalledOnce();
  });
});
