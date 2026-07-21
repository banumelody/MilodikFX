import { useState } from 'react';

export interface PresetControlsProps {
  presets: string[];
  selected: string;
  busy: boolean;
  onLoad: (name: string) => void;
  onSave: (name: string) => void;
  onDelete: (name: string) => void;
}

export function PresetControls({
  presets,
  selected,
  busy,
  onLoad,
  onSave,
  onDelete,
}: PresetControlsProps) {
  const [draftName, setDraftName] = useState('');

  const nameToSave = draftName.trim() || selected;

  return (
    <section className="panel" aria-label="Preset">
      <header className="panel__head">
        <h2 className="panel__title">Preset</h2>
      </header>

      <div className="preset">
        <select
          className="preset__select"
          value={selected}
          disabled={busy || presets.length === 0}
          onChange={(event) => onLoad(event.target.value)}
          aria-label="Pilih preset"
        >
          {presets.length === 0 ? <option value="">Belum ada preset</option> : null}
          {presets.map((name) => (
            <option key={name} value={name}>
              {name}
            </option>
          ))}
        </select>

        <div className="preset__row">
          <input
            className="preset__input"
            type="text"
            placeholder="Nama preset baru"
            value={draftName}
            disabled={busy}
            onChange={(event) => setDraftName(event.target.value)}
            onKeyDown={(event) => {
              if (event.key === 'Enter' && nameToSave) {
                onSave(nameToSave);
                setDraftName('');
              }
            }}
            aria-label="Nama preset"
          />
          <button
            type="button"
            className="btn"
            disabled={busy || !nameToSave}
            onClick={() => {
              onSave(nameToSave);
              setDraftName('');
            }}
          >
            Simpan
          </button>
          <button
            type="button"
            className="btn btn--danger"
            disabled={busy || !selected}
            onClick={() => onDelete(selected)}
          >
            Hapus
          </button>
        </div>
      </div>
    </section>
  );
}

export default PresetControls;
