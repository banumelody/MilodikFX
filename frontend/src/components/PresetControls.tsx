import { memo, useEffect, useMemo, useRef, useState } from 'react';

import type { PresetMetadata } from '../services/api';
import { useT } from '../i18n';

export interface PresetControlsProps {
  presets: string[];
  details?: PresetMetadata[];
  selected: string;
  busy: boolean;
  onLoad: (name: string) => void;
  onSave: (name: string) => void;
  onDelete: (name: string) => void;
  onMetadataChange?: (
    name: string,
    changes: Partial<Pick<PresetMetadata, 'description' | 'tags' | 'favourite' | 'notes'>>,
  ) => void;
  onExport?: (name: string) => void;
  onImport?: (name: string, data: string) => void;
}

const EMPTY: PresetMetadata = {
  name: '',
  description: '',
  tags: [],
  favourite: false,
  notes: '',
  savedAt: '',
};

function PresetControlsBase({
  presets,
  details = [],
  selected,
  busy,
  onLoad,
  onSave,
  onDelete,
  onMetadataChange,
  onExport,
  onImport,
}: PresetControlsProps) {
  const t = useT();

  const [draftName, setDraftName] = useState('');
  const [search, setSearch] = useState('');
  const [showNotes, setShowNotes] = useState(false);
  const fileInput = useRef<HTMLInputElement | null>(null);

  const nameToSave = draftName.trim() || selected;

  const byName = useMemo(
    () => new Map(details.map((entry) => [entry.name, entry])),
    [details],
  );

  const current = byName.get(selected) ?? EMPTY;

  // Local so typing does not fight the round trip; pushed on blur.
  const [notes, setNotes] = useState(current.notes);
  const [tags, setTags] = useState(current.tags.join(', '));

  useEffect(() => {
    setNotes(current.notes);
    setTags(current.tags.join(', '));
  }, [current.name, current.notes, current.tags]);

  // Filtering happens here rather than server-side: the whole list already
  // arrived with GET /api/presets, so a round trip per keystroke buys nothing.
  // Tags and description are searched too, which is what they are for.
  const visible = useMemo(() => {
    const needle = search.trim().toLowerCase();

    if (!needle) return presets;

    return presets.filter((name) => {
      if (name.toLowerCase().includes(needle)) return true;

      const entry = byName.get(name);

      if (!entry) return false;

      return (
        entry.description.toLowerCase().includes(needle) ||
        entry.tags.some((tag) => tag.toLowerCase().includes(needle))
      );
    });
  }, [presets, search, byName]);

  const decorate = (name: string) => {
    const entry = byName.get(name);
    if (!entry) return name;

    const suffix = entry.tags.length > 0 ? ` — ${entry.tags.join(', ')}` : '';
    return `${entry.favourite ? '★ ' : ''}${name}${suffix}`;
  };

  return (
    <section className="panel" aria-label="Preset">
      <header className="panel__head">
        <h2 className="panel__title">{t('preset.title')}</h2>
        <span className="panel__count">{presets.length}</span>
      </header>

      <div className="preset">
        {presets.length > 4 ? (
          <input
            className="preset__input"
            type="search"
            placeholder={t('preset.search')}
            value={search}
            disabled={busy}
            onChange={(event) => setSearch(event.target.value)}
            aria-label={t('preset.searchAria')}
          />
        ) : null}

        <select
          className="preset__select"
          size={Math.min(6, Math.max(2, visible.length))}
          value={selected}
          disabled={busy || visible.length === 0}
          onChange={(event) => onLoad(event.target.value)}
          aria-label={t('preset.selectAria')}
        >
          {visible.length === 0 ? (
            <option value="">
              {presets.length === 0 ? t('preset.none') : t('preset.noMatch')}
            </option>
          ) : null}
          {visible.map((name) => (
            <option key={name} value={name}>
              {decorate(name)}
            </option>
          ))}
        </select>

        <div className="preset__row">
          <input
            className="preset__input"
            type="text"
            placeholder={t('preset.newName')}
            value={draftName}
            disabled={busy}
            onChange={(event) => setDraftName(event.target.value)}
            onKeyDown={(event) => {
              if (event.key === 'Enter' && nameToSave) {
                onSave(nameToSave);
                setDraftName('');
              }
            }}
            aria-label={t('preset.nameAria')}
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
            {t('preset.save')}
          </button>
          <button
            type="button"
            className="btn btn--danger"
            disabled={busy || !selected}
            onClick={() => onDelete(selected)}
          >
            {t('preset.delete')}
          </button>
        </div>

        {selected && onMetadataChange ? (
          <div className="preset__row">
            <button
              type="button"
              className={`pill-btn${current.favourite ? ' pill-btn--active' : ''}`}
              disabled={busy}
              aria-pressed={current.favourite}
              aria-label={t('preset.favourite')}
              title={t('preset.markFavourite')}
              onClick={() => onMetadataChange(selected, { favourite: !current.favourite })}
            >
              {current.favourite ? '★' : '☆'}
            </button>

            <input
              className="preset__input"
              type="text"
              placeholder={t('preset.tagsPlaceholder')}
              value={tags}
              disabled={busy}
              aria-label={t('preset.tagsAria')}
              onChange={(event) => setTags(event.target.value)}
              onBlur={() =>
                onMetadataChange(selected, {
                  tags: tags
                    .split(',')
                    .map((tag) => tag.trim())
                    .filter(Boolean),
                })
              }
            />

            <button
              type="button"
              className="btn btn--ghost"
              disabled={busy}
              aria-pressed={showNotes}
              onClick={() => setShowNotes((open) => !open)}
            >
              {t('preset.notes')}
            </button>
          </div>
        ) : null}

        {selected && showNotes && onMetadataChange ? (
          <textarea
            className="preset__notes"
            rows={4}
            placeholder={t('preset.notesPlaceholder')}
            value={notes}
            disabled={busy}
            aria-label={t('preset.notesAria')}
            onChange={(event) => setNotes(event.target.value)}
            onBlur={() => onMetadataChange(selected, { notes })}
          />
        ) : null}

        {onExport && onImport ? (
          <div className="preset__row">
            <button
              type="button"
              className="btn btn--ghost"
              disabled={busy || !selected}
              onClick={() => onExport(selected)}
            >
              {t('preset.export')}
            </button>

            <button
              type="button"
              className="btn btn--ghost"
              disabled={busy}
              onClick={() => fileInput.current?.click()}
            >
              {t('preset.import')}
            </button>

            <input
              ref={fileInput}
              type="file"
              accept=".json,application/json"
              hidden
              aria-label={t('preset.fileAria')}
              onChange={(event) => {
                const file = event.target.files?.[0];
                if (!file) return;

                const reader = new FileReader();

                reader.onload = () => {
                  // The filename minus its extensions is the name it lands
                  // under; the engine falls back to the name inside the file.
                  const name = file.name.replace(/\.milodikfx\.json$/i, '').replace(/\.json$/i, '');
                  onImport(name, String(reader.result ?? ''));
                };

                reader.readAsText(file);

                // Cleared so re-picking the same file fires change again.
                event.target.value = '';
              }}
            />
          </div>
        ) : null}
      </div>
    </section>
  );
}

export const PresetControls = memo(PresetControlsBase);

export default PresetControls;
