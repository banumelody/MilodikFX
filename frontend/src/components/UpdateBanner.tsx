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
  if (!info || !info.updateAvailable) return null;

  return (
    <div className="banner banner--update" role="status">
      <span>
        Versi baru <strong>{info.latest}</strong> tersedia — kamu memakai v{info.current}.
      </span>
      <span className="banner__actions">
        <a href={info.url} target="_blank" rel="noreferrer" className="banner__link">
          Lihat rilis
        </a>
        <button
          type="button"
          className="banner__close"
          onClick={onDismiss}
          aria-label="Tutup pemberitahuan pembaruan"
        >
          ×
        </button>
      </span>
    </div>
  );
}

export default UpdateBanner;
