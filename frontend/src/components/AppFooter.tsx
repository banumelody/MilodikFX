/** Where the support and credit links point. Kept here so tests can assert them. */
export const SPONSOR_URL = 'https://github.com/sponsors/banumelody';
export const REPO_URL = 'https://github.com/banumelody/MilodikFX';

import { LANGUAGE_NAMES, LANGUAGES, useLanguage } from '../i18n';

export interface AppFooterProps {
  /** The running build's version, once the engine has reported it. */
  version?: string;
}

/**
 * Credit line and support links, always at the foot of the app.
 *
 * The links open in the system browser: inside the app the WebView routes a
 * target=_blank through newWindowAttemptingToLoad into the default browser, and
 * a real browser tab (dev) opens them directly.
 */
export function AppFooter({ version }: AppFooterProps) {
  const { language, setLanguage, t } = useLanguage();

  return (
    <footer className="appfooter">
      <div className="appfooter__credit">
        <span className="appfooter__app">MilodikFX{version ? ` v${version}` : ''}</span>
        <span className="appfooter__by">
          {t('footer.madeBy')} <strong>Banu Antoro</strong>
          {' · '}
          <a href={REPO_URL} target="_blank" rel="noreferrer">
            @banumelody
          </a>
        </span>
      </div>

      <div className="appfooter__actions">
        {/* A select rather than a flag: the app speaks two languages, and a flag
            stands for a country rather than for either of them. */}
        <label className="appfooter__lang">
          <span className="appfooter__lang-label">{t('app.language')}</span>
          <select
            value={language}
            aria-label={t('app.language')}
            onChange={(event) => setLanguage(event.target.value as typeof language)}
          >
            {LANGUAGES.map((code) => (
              <option key={code} value={code}>
                {LANGUAGE_NAMES[code]}
              </option>
            ))}
          </select>
        </label>

        <a className="appfooter__sponsor" href={SPONSOR_URL} target="_blank" rel="noreferrer">
          <span aria-hidden="true">☕</span> {t('footer.buyCoffee')}
        </a>
      </div>
    </footer>
  );
}

export default AppFooter;
