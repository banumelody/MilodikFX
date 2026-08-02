import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { beforeEach, describe, expect, it } from 'vitest';

import { LanguageProvider } from '../../i18n';
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

describe('the language picker', () => {
  beforeEach(() => {
    window.localStorage.clear();
  });

  const open = () =>
    render(
      <LanguageProvider>
        <AppFooter version="0.35.0" />
      </LanguageProvider>,
    );

  it('offers both languages under their own names', () => {
    open();

    const picker = screen.getByRole('combobox', { name: /bahasa/i });
    expect(picker).toHaveValue('id');
    expect(screen.getByRole('option', { name: 'Bahasa Indonesia' })).toBeInTheDocument();
    expect(screen.getByRole('option', { name: 'English' })).toBeInTheDocument();
  });

  it('re-words the footer itself when switched', async () => {
    const user = userEvent.setup();
    open();

    expect(screen.getByRole('link', { name: /traktir kopi/i })).toBeInTheDocument();

    await user.selectOptions(screen.getByRole('combobox', { name: /bahasa/i }), 'en');

    // The whole point: the picker is inside the thing it re-words, so if the
    // change did not reach the tree, this line still reads "Traktir kopi".
    expect(screen.getByRole('link', { name: /buy me a coffee/i })).toBeInTheDocument();
    expect(screen.queryByText(/traktir kopi/i)).not.toBeInTheDocument();
  });

  it('leaves the author and the version alone in both languages', async () => {
    const user = userEvent.setup();
    open();

    await user.selectOptions(screen.getByRole('combobox', { name: /language|bahasa/i }), 'en');

    // A name is a name, and a version is a number.
    expect(screen.getByText('Banu Antoro')).toBeInTheDocument();
    expect(screen.getByText('MilodikFX v0.35.0')).toBeInTheDocument();
  });
});
