import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { beforeEach, describe, expect, it } from 'vitest';

import { LanguageProvider, useLanguage, useT } from '..';

function Probe() {
  const { language, setLanguage } = useLanguage();
  const t = useT();

  return (
    <div>
      <p data-testid="line">{t('app.bypassNotice')}</p>
      <p data-testid="named">{t('scene.switchTo', { name: 'Solo' })}</p>
      <p data-testid="current">{language}</p>
      <button type="button" onClick={() => setLanguage(language === 'id' ? 'en' : 'id')}>
        switch
      </button>
    </div>
  );
}

describe('LanguageProvider', () => {
  beforeEach(() => {
    window.localStorage.clear();
    document.documentElement.lang = '';
  });

  it('speaks Indonesian until asked otherwise', () => {
    render(
      <LanguageProvider>
        <Probe />
      </LanguageProvider>,
    );

    expect(screen.getByTestId('current')).toHaveTextContent('id');
    expect(screen.getByTestId('line')).toHaveTextContent(/kamu mendengar sinyal kering/i);
  });

  it('re-words everything the moment the language changes', async () => {
    const user = userEvent.setup();

    render(
      <LanguageProvider>
        <Probe />
      </LanguageProvider>,
    );

    await user.click(screen.getByRole('button', { name: 'switch' }));

    expect(screen.getByTestId('current')).toHaveTextContent('en');
    expect(screen.getByTestId('line')).toHaveTextContent(/you are hearing the dry signal/i);
  });

  it('remembers the choice for the next session', async () => {
    const user = userEvent.setup();

    render(
      <LanguageProvider>
        <Probe />
      </LanguageProvider>,
    );

    await user.click(screen.getByRole('button', { name: 'switch' }));

    expect(window.localStorage.getItem('milodikfx.language')).toBe('en');
  });

  it('reads the stored choice back at start-up', () => {
    window.localStorage.setItem('milodikfx.language', 'en');

    render(
      <LanguageProvider>
        <Probe />
      </LanguageProvider>,
    );

    expect(screen.getByTestId('current')).toHaveTextContent('en');
  });

  it('ignores a stored language it does not speak', () => {
    // A hand-edited value, or one left by a build that offered more languages.
    // Falling through to the default beats rendering every string as its key.
    window.localStorage.setItem('milodikfx.language', 'jp');

    render(
      <LanguageProvider>
        <Probe />
      </LanguageProvider>,
    );

    expect(screen.getByTestId('current')).toHaveTextContent('id');
  });

  it('declares the language on the document, from the first render', async () => {
    const user = userEvent.setup();

    render(
      <LanguageProvider>
        <Probe />
      </LanguageProvider>,
    );

    // Not only after the picker is touched: a session that never opens it still
    // has to announce itself correctly to a screen reader.
    expect(document.documentElement.lang).toBe('id');

    await user.click(screen.getByRole('button', { name: 'switch' }));
    expect(document.documentElement.lang).toBe('en');
  });

  it('fills placeholders in whichever language is current', async () => {
    const user = userEvent.setup();

    render(
      <LanguageProvider>
        <Probe />
      </LanguageProvider>,
    );

    expect(screen.getByTestId('named')).toHaveTextContent('Pindah ke Solo');

    await user.click(screen.getByRole('button', { name: 'switch' }));
    expect(screen.getByTestId('named')).toHaveTextContent('Switch to Solo');
  });

  it('renders words rather than keys outside a provider', () => {
    // A component rendered in isolation -- by a test, or by a future screen that
    // forgets the provider -- must still be readable.
    render(<Probe />);

    expect(screen.getByTestId('line')).toHaveTextContent(/kamu mendengar sinyal kering/i);
  });
});
