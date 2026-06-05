# MilodikFX UI/UX Layout v0.1

## 1. UI Concept

MilodikFX menggunakan konsep desktop guitar processor dengan layout seperti pedalboard modern.

Prioritas UI v0.1:

- Cepat dipahami
- Ringan untuk dibuat di JUCE
- Mudah diuji
- Fokus pada realtime control
- Tidak terlalu artistik dulu

---

# 2. Main Layout

```text
┌──────────────────────────────────────────────────────────────┐
│ MilodikFX                         CPU 4% | 48kHz | 128 samples │
├──────────────────────────────────────────────────────────────┤
│ Preset: [Default Clean ▼]  [Save] [Load] [Delete]             │
├──────────────────────────────────────────────────────────────┤
│ Input Device : [Focusrite ASIO Input ▼]                      │
│ Output Device: [Focusrite ASIO Output ▼]                     │
│ Input Level  :  ███████░░░                                  │
│ Output Level :  ██████░░░░                                  │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌────────────┐   ┌────────────┐   ┌────────────┐             │
│  │ CLEAN BOOST│ → │ OVERDRIVE  │ → │   3 BAND EQ│             │
│  │            │   │            │   │            │             │
│  │ Gain       │   │ Drive      │   │ Bass       │             │
│  │   (knob)   │   │   (knob)   │   │  (knob)    │             │
│  │            │   │ Level      │   │ Mid        │             │
│  │ [ON/OFF]   │   │   (knob)   │   │  (knob)    │             │
│  │            │   │ [ON/OFF]   │   │ Treble     │             │
│  └────────────┘   └────────────┘   │  (knob)    │             │
│                                   │ [ON/OFF]    │             │
│                                   └────────────┘             │
│                                                              │
├──────────────────────────────────────────────────────────────┤
│ Master Volume: (knob)        Bypass: [OFF]        Status: OK  │
└──────────────────────────────────────────────────────────────┘
```

---

# 3. Main Sections

## 3.1 Top Bar

Isi:

- Logo / nama aplikasi
- CPU usage
- Sample rate
- Buffer size
- Audio status

Contoh:

```text
MilodikFX | CPU 4% | 48kHz | 128 samples | ASIO Ready
```

---

## 3.2 Preset Bar

Fungsi:

- Memilih preset
- Menyimpan preset
- Menghapus preset

Komponen:

- Preset Dropdown
- Save Button
- Load Button
- Delete Button

---

## 3.3 Audio Device Panel

Fungsi:

- Memilih input device
- Memilih output device
- Melihat input/output level

Komponen:

- Input Device Dropdown
- Output Device Dropdown
- Input Level Meter
- Output Level Meter

---

## 3.4 Pedalboard Area

Area utama untuk effect chain.

Urutan v0.1:

```text
Clean Boost → Overdrive → 3 Band EQ
```

Setiap effect card memiliki:

- Nama effect
- Parameter knob
- ON/OFF toggle
- Status indicator

---

## 3.5 Bottom Control Bar

Isi:

- Master Volume
- Global Bypass
- Status Message

Contoh:

```text
Master Volume | Bypass | Status: Audio Running
```

---

# 4. Effect Card Layout

## Clean Boost

```text
┌──────────────┐
│ CLEAN BOOST  │
│              │
│ Gain         │
│   [ knob ]   │
│              │
│ [ ON/OFF ]   │
└──────────────┘
```

Parameter:

- Gain: 0 dB sampai 24 dB

---

## Overdrive

```text
┌──────────────┐
│ OVERDRIVE    │
│              │
│ Drive        │
│   [ knob ]   │
│              │
│ Level        │
│   [ knob ]   │
│              │
│ [ ON/OFF ]   │
└──────────────┘
```

Parameter:

- Drive: 0 sampai 100
- Level: 0 sampai 100

---

## 3 Band EQ

```text
┌──────────────┐
│ 3 BAND EQ    │
│              │
│ Bass         │
│   [ knob ]   │
│ Mid          │
│   [ knob ]   │
│ Treble       │
│   [ knob ]   │
│              │
│ [ ON/OFF ]   │
└──────────────┘
```

Parameter:

- Bass: -12 dB sampai +12 dB
- Mid: -12 dB sampai +12 dB
- Treble: -12 dB sampai +12 dB

---

# 5. UX Flow

## First Launch

1. User membuka MilodikFX.
2. Aplikasi mendeteksi audio device.
3. User memilih ASIO device.
4. User memilih input dan output.
5. User memainkan gitar.
6. Input meter bergerak.
7. Output terdengar.

---

## Basic Tone Editing

1. User menyalakan Clean Boost.
2. User mengatur Gain.
3. User menyalakan Overdrive.
4. User mengatur Drive dan Level.
5. User membentuk tone dengan EQ.
6. User menyimpan preset.

---

# 6. UI Priority by Sprint

## Sprint 0

Wajib ada:

- Audio Device Dropdown
- Input Level Meter
- Output Level Meter
- Audio Status Text

Belum perlu:

- Knob
- Preset
- Effect Card

---

## Sprint 1

Tambah:

- Empty DSP Chain View
- Global Bypass

---

## Sprint 2

Tambah:

- Clean Boost Card
- Gain Knob
- ON/OFF Toggle

---

## Sprint 3

Tambah:

- Overdrive Card
- Drive Knob
- Level Knob

---

## Sprint 4

Tambah:

- EQ Card
- Bass Knob
- Mid Knob
- Treble Knob

---

## Sprint 5

Tambah:

- Preset Dropdown
- Save Button
- Load Button
- Delete Button

---

# 7. Visual Style Direction

## Theme

Dark modern audio interface.

## Mood

- Professional
- Clean
- Technical
- Musical

## Color Direction

- Background: dark charcoal
- Card: dark grey
- Accent: electric blue or amber
- Text: soft white
- Warning: orange/red
- Active status: green

---

# 8. Design Principle

MilodikFX v0.1 harus terasa seperti:

```text
Developer tool + guitar processor
```

Bukan seperti:

```text
Mainan efek gitar
```

Prioritas:

1. Audio working first
2. Controls responsive
3. Low CPU usage
4. Clear signal flow
5. Easy debugging

---

# 9. Future UI Ideas

Untuk versi berikutnya:

- Drag and drop effect chain
- Pedalboard visual mode
- Amp head visual mode
- Cabinet IR browser
- Spectrum analyzer
- Tuner
- MIDI mapping panel
- Performance mode
- Fullscreen live mode
- Touchscreen mode
