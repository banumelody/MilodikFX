import { describe, expect, it } from 'vitest';

import { DICTIONARIES } from '../chain';
import { LANGUAGES, STRINGS } from '../strings';
import type { Language, StringKey } from '../strings';

const keys = Object.keys(STRINGS.id) as StringKey[];

/**
 * Every word the app can show, in one language, as one string.
 *
 * Both dictionaries, deliberately. The block descriptions live in `chain.ts`
 * rather than in `STRINGS`, and they are exactly where "head" and "cabinet"
 * appear -- a guard that read only `STRINGS` passed while `chain.ts` said
 * "Kepala amp".
 */
function everythingSaidIn(language: Language): string {
  const chain = DICTIONARIES[language];

  return [
    ...Object.values(STRINGS[language]),
    ...Object.values(chain.descriptions),
    ...Object.values(chain.parameterLabels),
    ...Object.values(chain.enumOptions).flat(),
  ]
    .join(' · ')
    .toLowerCase();
}

describe('the dictionary', () => {
  it('says everything in every language it claims to speak', () => {
    // TypeScript already requires the English record to have the Indonesian
    // record's keys. What it cannot see is an entry left as an empty string,
    // which reads on screen as a missing label rather than as an error.
    for (const language of LANGUAGES) {
      for (const key of keys) {
        expect(STRINGS[language][key], `${language}/${key}`).toBeTruthy();
      }
    }
  });

  it('carries the same placeholders through every translation', () => {
    // A translation that drops {name} loses the only part of the sentence that
    // was not written in advance, and one that invents {nama} silently renders
    // the braces. Both are invisible until the string happens to be shown.
    const placeholders = (value: string) =>
      (value.match(/\{(\w+)\}/g) ?? []).slice().sort().join(',');

    for (const key of keys) {
      const reference = placeholders(STRINGS.id[key]);

      for (const language of LANGUAGES) {
        expect(placeholders(STRINGS[language][key]), `${language}/${key}`).toBe(reference);
      }
    }
  });
});

/**
 * The one rule the Indonesian side has to keep: the words printed on the gear
 * stay the words printed on the gear.
 *
 * Each of these appears verbatim in the Indonesian dictionary today, so a
 * well-meant future edit that turns "head" into "kepala" fails here rather than
 * ships. Only terms whose *sense* is unambiguous are listed -- "release" is not,
 * because a software release is a different word that happens to be spelled the
 * same, and matching it would assert nothing.
 */
const KEEP = [
  'overdrive',
  'cabinet',
  'head',
  'delay',
  'damping',
  'impulse response',
  'limiter',
  'preset',
  'scene',
  'bypass',
  'mute',
  'buffer',
  'sample rate',
  'footswitch',
  'looper',
  'overdub',
  'tuner',
  'modifier',
  'feedback',
  'level',
];

/** What an over-eager translation would have turned each of them into. */
const FORBIDDEN: Record<string, string[]> = {
  head: ['kepala'],
  cabinet: ['lemari', 'kabinet'],
  gain: ['penguatan', 'perolehan'],
  threshold: ['ambang batas'],
  attack: ['serangan'],
  release: ['pelepasan', 'pembebasan'],
  ratio: ['rasio perbandingan'],
  mix: ['campuran'],
  feedback: ['umpan balik'],
  preset: ['pratata', 'prasetel'],
  scene: ['adegan', 'pemandangan'],
  channel: ['saluran'],
  bypass: ['lewati', 'pintasan'],
  mute: ['bisukan suara'],
  buffer: ['penyangga', 'penyangga memori'],
  looper: ['pengulang'],
  footswitch: ['sakelar kaki', 'saklar kaki'],
  'impulse response': ['tanggapan impuls', 'respons impuls'],
  'noise gate': ['gerbang derau', 'gerbang bising'],
  'sample rate': ['laju cuplikan', 'laju sampel'],
};

describe('technical vocabulary in Indonesian', () => {
  const indonesian = everythingSaidIn('id');

  it.each(Object.entries(FORBIDDEN))('leaves %s alone', (term, translations) => {
    for (const translation of translations) {
      expect(indonesian, `"${term}" was translated as "${translation}"`).not.toContain(translation);
    }
  });

  it.each(KEEP)('still says %s', (term) => {
    // The other half of the rule, and the half that catches a term quietly
    // paraphrased away rather than mistranslated outright.
    expect(indonesian, `"${term}" is missing from the Indonesian side`).toContain(term);
  });
});

describe('the two languages', () => {
  it('agree on the words that are not words but names', () => {
    // Product and protocol names -- these are the same string in both, and a
    // translation of one of them would be a bug, not a nicety.
    const same: StringKey[] = ['app.perform', 'app.edit', 'app.bypass', 'mod.sync', 'mod.rateHz'];

    for (const key of same) {
      expect(STRINGS.en[key]).toBe(STRINGS.id[key]);
    }
  });

  it('offers exactly the languages it has dictionaries for', () => {
    expect(LANGUAGES.slice().sort()).toEqual((['en', 'id'] as Language[]).sort());
    expect(Object.keys(STRINGS).sort()).toEqual(['en', 'id']);
  });
});
