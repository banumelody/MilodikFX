import { memo, useCallback, useEffect, useRef, useState } from 'react';

import { getNamLibrary, importNam, revealNamFolder } from '../services/api';
import type { NamLibraryResponse } from '../services/api';

interface NamPanelProps {
  disabled?: boolean;
  /** Called after a model is imported so the Amp card's chooser refreshes. */
  onLibraryChanged: () => void;
}

/**
 * Manages the NAM amp-head models on disk.
 *
 * Choosing which model the Amp stage runs is on the Amp card itself (a text
 * parameter like the impulse-response chooser). This panel only handles getting
 * files onto disk, and — importantly — says when models cannot be run at all,
 * so a build without NAM or a CPU without AVX2 explains itself rather than
 * failing a load silently.
 */
function NamPanelBase({ disabled = false, onLibraryChanged }: NamPanelProps) {
  const [library, setLibrary] = useState<NamLibraryResponse | null>(null);
  const [error, setError] = useState<string | null>(null);
  const fileInput = useRef<HTMLInputElement | null>(null);
  const inFlight = useRef(false);

  const refresh = useCallback(async () => {
    try {
      setLibrary(await getNamLibrary());
      setError(null);
    } catch (caught) {
      setError(caught instanceof Error ? caught.message : String(caught));
    }
  }, []);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  const handleReveal = useCallback(async () => {
    try {
      await revealNamFolder();
      // The folder is open; pick up whatever is dropped in shortly after.
      window.setTimeout(() => void refresh(), 1500);
    } catch (caught) {
      setError(caught instanceof Error ? caught.message : String(caught));
    }
  }, [refresh]);

  const handleFile = useCallback(
    (file: File) => {
      if (inFlight.current) return;
      inFlight.current = true;

      const reader = new FileReader();

      reader.onload = () => {
        void (async () => {
          try {
            // The engine wants base64, not the data: URL prefix FileReader adds.
            const result = String(reader.result ?? '');
            const base64 = result.slice(result.indexOf(',') + 1);
            const name = file.name.replace(/\.nam$/i, '');

            await importNam(name, base64);
            await refresh();
            onLibraryChanged();
            setError(null);
          } catch (caught) {
            setError(caught instanceof Error ? caught.message : String(caught));
          } finally {
            inFlight.current = false;
          }
        })();
      };

      reader.readAsDataURL(file);
    },
    [refresh, onLibraryChanged],
  );

  const available = library?.available ?? true;

  return (
    <section className="panel" aria-label="Model NAM">
      <header className="panel__head">
        <h2 className="panel__title">Amp Model (NAM)</h2>
        <span className="panel__count">{library?.models.length ?? 0}</span>
      </header>

      {!available && library ? (
        <p className="panel__error" role="alert">
          {library.unavailableReason || 'Model NAM tidak bisa dijalankan di sistem ini.'}
        </p>
      ) : null}

      <p className="panel__hint">
        Letakkan berkas <code>.nam</code> hasil capture (mis. dari TONE3000) di folder model, lalu
        pilih pada kartu Amp (NAM). Tanpa model, stage ini meneruskan sinyal apa adanya.
      </p>

      <div className="preset__row">
        <button type="button" className="btn btn--ghost" disabled={disabled} onClick={() => void handleReveal()}>
          Buka folder
        </button>

        <button
          type="button"
          className="btn btn--ghost"
          disabled={disabled || !available}
          onClick={() => fileInput.current?.click()}
        >
          Impor model
        </button>

        <input
          ref={fileInput}
          type="file"
          accept=".nam"
          hidden
          aria-label="Berkas model NAM"
          onChange={(event) => {
            const file = event.target.files?.[0];
            if (file) handleFile(file);
            event.target.value = '';
          }}
        />
      </div>

      {error ? (
        <p className="panel__error" role="alert">
          {error}
        </p>
      ) : null}
    </section>
  );
}

export const NamPanel = memo(NamPanelBase);

export default NamPanel;
