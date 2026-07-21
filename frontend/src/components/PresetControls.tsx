import { useMemo, useState } from 'react';

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
  const [search, setSearch] = useState('');

  const nameToSave = draftName.trim() || selected;

  // Filtering happens here rather than server-side: the whole list already
  // arrived with GET /api/presets, so a round trip per keystroke buys nothing.
  const visible = useMemo(() => {
    const needle = search.trim().toLowerCase();
    if (!needle) return presets;
    return presets.filter((name) => name.toLowerCase().includes(needle));
  }, [presets, search]);

  return (
    <section className="panel" aria-label="Preset">
      <header className="panel__head">
        <h2 className="panel__title">Preset</h2>
        <span className="panel__count">{presets.length}</span>
      </header>

      <div className="preset">
        {presets.length > 4 ? (
          <input
            className="preset__input"
            type="search"
            placeholder="Cari preset"
            value={search}
            disabled={busy}
            onChange={(event) => setSearch(event.target.value)}
            aria-label="Cari preset"
          />
        ) : null}

        <select
          className="preset__select"
          size={Math.min(6, Math.max(2, visible.length))}
          value={selected}
          disabled={busy || visible.length === 0}
          onChange={(event) => onLoad(event.target.value)}
          aria-label="Pilih preset"
        >
          {visible.length === 0 ? (
            <option value="">{presets.length === 0 ? 'Belum ada preset' : 'Tidak ada yang cocok'}</option>
          ) : null}
          {visible.map((name) => (
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
