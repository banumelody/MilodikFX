/** Where the support and credit links point. Kept here so tests can assert them. */
export const SPONSOR_URL = 'https://github.com/sponsors/banumelody';
export const REPO_URL = 'https://github.com/banumelody/MilodikFX';

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
  return (
    <footer className="appfooter">
      <div className="appfooter__credit">
        <span className="appfooter__app">MilodikFX{version ? ` v${version}` : ''}</span>
        <span className="appfooter__by">
          Dibuat oleh <strong>Banu Antoro</strong>
          {' · '}
          <a href={REPO_URL} target="_blank" rel="noreferrer">
            @banumelody
          </a>
        </span>
      </div>

      <a className="appfooter__sponsor" href={SPONSOR_URL} target="_blank" rel="noreferrer">
        <span aria-hidden="true">☕</span> Traktir kopi
      </a>
    </footer>
  );
}

export default AppFooter;
