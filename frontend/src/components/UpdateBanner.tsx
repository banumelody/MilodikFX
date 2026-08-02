import { useT } from '../i18n';
import type { UpdateInfo } from '../services/api';

export interface UpdateBannerProps {
  /** Null hides the banner; the parent nulls it once dismissed for this version. */
  info: UpdateInfo | null;
  onDismiss: () => void;
}

/**
 * Announces a newer release, without nagging. The parent remembers the dismissal
 * per version, so once closed it returns only when a still-newer build appears.
 * Renders nothing unless the engine actually confirmed an update.
 */
export function UpdateBanner({ info, onDismiss }: UpdateBannerProps) {
  const t = useT();

  if (!info || !info.updateAvailable) return null;

  return (
    <div className="banner banner--update" role="status">
      <span>
        {t('update.newVersionBefore')} <strong>{info.latest}</strong>{' '}
        {t('update.newVersionAfter', { current: info.current })}
      </span>
      <span className="banner__actions">
        <a href={info.url} target="_blank" rel="noreferrer" className="banner__link">
          {t('update.view')}
        </a>
        <button
          type="button"
          className="banner__close"
          onClick={onDismiss}
          aria-label={t('update.dismissAria')}
        >
          ×
        </button>
      </span>
    </div>
  );
}

export default UpdateBanner;
