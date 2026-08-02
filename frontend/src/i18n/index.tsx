import { createContext, useCallback, useContext, useEffect, useMemo, useState } from 'react';

import { LANGUAGES, STRINGS } from './strings';
import type { Language, StringKey } from './strings';

export { LANGUAGES, LANGUAGE_NAMES } from './strings';
export type { Language, StringKey } from './strings';
export { effectDescription, enumLabels, parameterLabel, DRIVE_VOICINGS } from './chain';

const STORAGE_KEY = 'milodikfx.language';

/**
 * Indonesian by default, because that is what the app has always spoken and a
 * silent switch on update would be a surprise. Read once at start-up; the
 * choice is a UI preference, so it lives in localStorage rather than travelling
 * in a preset or the settings file the engine owns.
 */
function readStored(): Language {
  try {
    const stored = window.localStorage.getItem(STORAGE_KEY);
    if (stored && (LANGUAGES as readonly string[]).includes(stored)) return stored as Language;
  } catch {
    /* a storage-less environment is not a reason to fail to render */
  }

  return 'id';
}

export type Translate = (key: StringKey, values?: Record<string, string | number>) => string;

interface LanguageValue {
  language: Language;
  setLanguage: (next: Language) => void;
  t: Translate;
}

const LanguageContext = createContext<LanguageValue | null>(null);

export function LanguageProvider({ children }: { children: React.ReactNode }) {
  const [language, setLanguageState] = useState<Language>(readStored);

  const setLanguage = useCallback((next: Language) => {
    setLanguageState(next);

    try {
      window.localStorage.setItem(STORAGE_KEY, next);
    } catch {
      /* the choice still applies for this session */
    }
  }, []);

  // So screen readers and the browser announce the page in the right language.
  // In an effect rather than inside setLanguage, or a session that never touched
  // the picker would keep whatever index.html happened to declare.
  useEffect(() => {
    document.documentElement.lang = language;
  }, [language]);

  const t = useCallback<Translate>(
    (key, values) => {
      const template = STRINGS[language][key] ?? STRINGS.id[key] ?? key;

      if (!values) return template;

      // {name}-style placeholders, replaced in one pass. Deliberately not a
      // template engine: every string here is a sentence, not a program.
      return template.replace(/\{(\w+)\}/g, (whole, name: string) =>
        name in values ? String(values[name]) : whole,
      );
    },
    [language],
  );

  const value = useMemo(() => ({ language, setLanguage, t }), [language, setLanguage, t]);

  return <LanguageContext.Provider value={value}>{children}</LanguageContext.Provider>;
}

/**
 * The translator, plus the current language for the chain dictionaries.
 *
 * Falls back to Indonesian outside a provider rather than throwing: a component
 * rendered in isolation by a test should show words, not a stack trace.
 */
export function useLanguage(): LanguageValue {
  const context = useContext(LanguageContext);

  if (context != null) return context;

  return {
    language: 'id',
    setLanguage: () => {},
    t: (key, values) => {
      const template = STRINGS.id[key] ?? key;
      if (!values) return template;
      return template.replace(/\{(\w+)\}/g, (whole, name: string) =>
        name in values ? String(values[name]) : whole,
      );
    },
  };
}

/** Shorthand for the common case. */
export function useT(): Translate {
  return useLanguage().t;
}
