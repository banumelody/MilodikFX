import type { Language } from './strings';

/**
 * What each block does, and the handful of control labels that were words
 * rather than terms.
 *
 * The engine still sends a label and a description with every effect -- they are
 * the fallback, and they are what a DAW shows in its automation lane, where a
 * runtime language switch is not possible anyway. What the UI *displays* comes
 * from here, keyed by block type, so `overdrive2` reads exactly as `overdrive`
 * does.
 *
 * **Technical vocabulary is left alone.** Overdrive, compressor, noise gate,
 * cabinet, head, delay, reverb, gain, threshold, attack, release, ratio, mix,
 * feedback, spillover, crossover, oversampling, impulse response -- these are
 * the words on the gear and in the manuals. Translating them would make the app
 * harder to read, not friendlier. Only the sentences around them change.
 */
type Descriptions = Record<string, string>;

const descriptions: Record<Language, Descriptions> = {
  id: {
    global: 'Kontrol yang berlaku untuk seluruh rantai',
    input: 'Samakan level gitar ini dengan rantai — setel sekali per gitar',
    noiseGate: 'Meredam dengung pickup di sela nada',
    cleanBoost: 'Dorong front-end untuk solo — hanya menambah, setelah noise gate',
    compressor: 'Meratakan dinamika petikan',
    split: 'Belah sinyal jadi dua jalur — A dan B',
    mixer: 'Gabungkan jalur A dan B — level dan pan tiap jalur',
    overdrive: 'Overdrive, distorsi, dan fuzz — pilih voicing pedalnya, kontrol menyesuaikan tipe',
    eq: 'Pembentuk nada SEBELUM drive',
    toneStack: 'Pembentuk nada SETELAH drive, sebelum cabinet',
    nam: 'Head amp hasil capture Neural Amp Modeler',
    cabinet: 'Simulasi speaker — biarkan analitik, atau muat impulse response',
    delay: 'Delay dengan feedback, damping, dan sinkron tempo',
    reverb: 'Ruang gema bergaya Freeverb, atau impulse response',
    master: 'Level keluaran dan limiter pengaman',
    metronome: 'Klik latihan, dicampur setelah master (tidak lewat rantai efek)',
  },
  en: {
    global: 'Controls that belong to the whole chain',
    input: 'Match this guitar to the chain — set once per guitar',
    noiseGate: 'Silences pickup hum between notes',
    cleanBoost: 'Pushes the front end for a solo — only adds, and sits after the noise gate',
    compressor: 'Evens out picking dynamics',
    split: 'Divides the signal into two paths — A and B',
    mixer: 'Brings paths A and B back together — level and pan for each',
    overdrive: 'Overdrive, distortion and fuzz — pick the pedal voicing and the controls follow',
    eq: 'Tone shaping BEFORE the drive',
    toneStack: 'Tone shaping AFTER the drive, before the cabinet',
    nam: 'An amp head captured with Neural Amp Modeler',
    cabinet: 'Speaker simulation — leave it analytic, or load an impulse response',
    delay: 'Feedback delay with damping and tempo sync',
    reverb: 'A Freeverb-style room, or an impulse response',
    master: 'Output level and the safety limiter',
    metronome: 'A practice click, mixed in after the master (not through the chain)',
  },
};

/**
 * Control labels that were sentences rather than terms.
 *
 * Everything not listed here -- Drive, Tone, Level, Threshold, Attack, Release,
 * Ratio, Mix, Feedback, Damping, Presence, Ceiling -- is already the word the
 * hardware prints on its own panel, and stays that way in both languages.
 */
const parameterLabels: Record<Language, Record<string, string>> = {
  id: {
    'split.freqHz': 'Frekuensi',
    'input.trimLink': 'Kaitkan L/R',
    'metronome.beatsPerBar': 'Ketukan/Bar',
    'cabinet.irMode': 'Mode IR',
    'cabinet.irEnabled': 'Pakai IR',
    'overdrive.type': 'Tipe',
  },
  en: {
    'split.freqHz': 'Frequency',
    'input.trimLink': 'Link L/R',
    'metronome.beatsPerBar': 'Beats/Bar',
    'cabinet.irMode': 'IR mode',
    'cabinet.irEnabled': 'Use IR',
    'overdrive.type': 'Type',
  },
};

/** Numeric parameters that are really a choice, per language. */
const enumOptions: Record<Language, Record<string, string[]>> = {
  id: {
    'input.mode': ['Mono - Input 1', 'Mono - Input 2', 'Mono - jumlah keduanya', 'Stereo'],
    'overdrive.oversampling': ['Mati', '2x', '4x', '8x'],
    'cabinet.irMode': ['Blend A+B', 'Stereo (A kiri / B kanan)'],
    'delay.syncMode': ['Mati', '1/4', '1/8.', '1/8', '1/8T', '1/16'],
    'split.mode': [
      'Sama ke dua jalur',
      'Crossover - low ke A, high ke B',
      'L/R - kanal L ke A, kanal R ke B',
    ],
    'mixer.invertB': ['Normal', 'Dibalik'],
  },
  en: {
    'input.mode': ['Mono - Input 1', 'Mono - Input 2', 'Mono - sum of both', 'Stereo'],
    'overdrive.oversampling': ['Off', '2x', '4x', '8x'],
    'cabinet.irMode': ['Blend A+B', 'Stereo (A left / B right)'],
    'delay.syncMode': ['Off', '1/4', '1/8.', '1/8', '1/8T', '1/16'],
    'split.mode': [
      'Same to both paths',
      'Crossover - lows to A, highs to B',
      'L/R - left channel to A, right to B',
    ],
    'mixer.invertB': ['Normal', 'Inverted'],
  },
};

/** The voicing names. Product names, so identical in both languages. */
export const DRIVE_VOICINGS = [
  'Custom',
  'Tube Screamer',
  'Bluesbreaker',
  'Blues Driver',
  'Transparent',
  'OCD',
  'Dumble',
  'Marshall-in-a-Box',
  'Clean Boost',
  'Centaur',
  'RAT',
  'Big Muff',
];

/**
 * The three tables together, per language.
 *
 * Exported so a test can hold the app's whole vocabulary at once: the rule that
 * technical terms stay untranslated is about all of the words, and a check that
 * saw only `STRINGS` would pass while this file said "Kepala amp".
 */
export const DICTIONARIES: Record<
  Language,
  { descriptions: Descriptions; parameterLabels: Record<string, string>; enumOptions: Record<string, string[]> }
> = {
  id: {
    descriptions: descriptions.id,
    parameterLabels: parameterLabels.id,
    enumOptions: { ...enumOptions.id, 'overdrive.type': DRIVE_VOICINGS },
  },
  en: {
    descriptions: descriptions.en,
    parameterLabels: parameterLabels.en,
    enumOptions: { ...enumOptions.en, 'overdrive.type': DRIVE_VOICINGS },
  },
};

export function effectDescription(language: Language, type: string, fallback: string) {
  return descriptions[language][type] ?? fallback;
}

export function parameterLabel(language: Language, type: string, id: string, fallback: string) {
  return parameterLabels[language][`${type}.${id}`] ?? fallback;
}

export function enumLabels(language: Language, key: string): string[] | undefined {
  if (key === 'overdrive.type') return DRIVE_VOICINGS;
  return enumOptions[language][key];
}
