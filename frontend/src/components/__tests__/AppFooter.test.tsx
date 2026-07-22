import { render, screen } from '@testing-library/react';
import { describe, expect, it } from 'vitest';

import { AppFooter, SPONSOR_URL } from '../AppFooter';

describe('AppFooter', () => {
  it('credits the author by name and nickname', () => {
    render(<AppFooter />);

    expect(screen.getByText('Banu Antoro')).toBeInTheDocument();
    expect(screen.getByRole('link', { name: '@banumelody' })).toBeInTheDocument();
  });

  it('offers a sponsor link that points at GitHub Sponsors', () => {
    render(<AppFooter />);

    const sponsor = screen.getByRole('link', { name: /traktir kopi/i });
    expect(sponsor).toHaveAttribute('href', SPONSOR_URL);
    // It must open outside the WebView, not navigate the control surface away.
    expect(sponsor).toHaveAttribute('target', '_blank');
  });

  it('shows the running version when the engine has reported one', () => {
    render(<AppFooter version="0.15.0" />);

    expect(screen.getByText('MilodikFX v0.15.0')).toBeInTheDocument();
  });

  it('shows the name alone before a version is known', () => {
    render(<AppFooter />);

    expect(screen.getByText('MilodikFX')).toBeInTheDocument();
  });
});
