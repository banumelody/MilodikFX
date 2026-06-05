# MilodikFX PRD v0.1

## Product Requirements Document

**Project Name:** MilodikFX
**Version:** 0.1
**Status:** Draft
**Target Platform:** Windows 11
**Author:** Banu Antoro

---

# 1. Executive Summary

MilodikFX adalah aplikasi multi-effect gitar dan bass berbasis DSP realtime yang dikembangkan menggunakan C++ dan JUCE.

Visi jangka panjang MilodikFX adalah menjadi platform audio processing profesional yang mampu bersaing dengan solusi kelas dunia seperti Quad Cortex, FM9, Helix, Kemper, dan Neural DSP pada platform desktop maupun hardware khusus.

Fase awal berfokus pada pengembangan engine DSP berkualitas tinggi pada platform Windows dengan latensi rendah dan kualitas audio profesional.

---

# 2. Product Vision

Membangun platform audio processing modern yang:

- Open architecture
- High quality audio processing
- Low latency
- Modular DSP engine
- Cross platform
- Hardware agnostic
- Siap dikembangkan menjadi plugin, desktop application, maupun dedicated hardware

---

# 3. Product Goals

## Primary Goals

### G-001

Membuat engine audio realtime yang stabil.

### G-002

Membuat framework DSP modular yang mudah dikembangkan.

### G-003

Membangun fondasi amp simulator modern.

### G-004

Membangun fondasi untuk tone capture dan AI processing di masa depan.

---

## Non Goals (v0.1)

Belum menjadi fokus:

- AI Tone Matching
- Neural Capture
- Cloud Preset
- Mobile Application
- Linux Support
- macOS Support
- Dedicated Hardware
- Plugin VST3
- Plugin AU

---

# 4. Product Scope

## Included

### Audio Device Management

- Input Device Selection
- Output Device Selection
- Sample Rate Selection
- Buffer Size Selection

### Realtime Audio Monitoring

- Low latency monitoring
- Audio level monitoring

### DSP Processing

- Clean Boost
- Overdrive
- 3 Band EQ

### Preset Management

- Save Preset
- Load Preset
- Delete Preset

---

## Excluded

- Delay
- Reverb
- Chorus
- Flanger
- Compressor
- Noise Gate
- Amp Simulation
- Cabinet Simulation
- IR Loader

---

# 5. Target User

## Primary User

Developer-musician yang ingin:

- Mempelajari audio DSP
- Mengembangkan multi-effect sendiri
- Bereksperimen dengan tone gitar
- Membuat platform audio profesional

---

# 6. Technical Requirements & Technology Stack

Bagian ini mendefinisikan standar teknis yang wajib digunakan selama pengembangan agar proyek tetap konsisten dan mudah dikembangkan di masa depan.

## Programming Language

### Primary Language

- C++20

Alasan:

- Performa tinggi untuk DSP realtime
- Dukungan luas pada industri audio profesional
- Kompatibel dengan JUCE
- Cocok untuk pengembangan desktop maupun embedded hardware

---

## Audio Application Framework

### JUCE

Versi minimum:

- JUCE 8.x atau versi stabil terbaru

JUCE digunakan untuk:

- Audio Device Management
- Audio Callback
- DSP Utilities
- GUI Framework
- Cross-platform Support
- Plugin Framework (fase berikutnya)

Alasan pemilihan:

- Standar industri audio software
- Digunakan oleh banyak produk profesional
- Dokumentasi dan komunitas kuat
- Mempermudah migrasi ke VST3, AU, dan standalone application

---

## Build System

### CMake

Versi minimum:

- CMake 3.25+

Digunakan untuk:

- Build automation
- Dependency management
- Cross-platform build configuration

---

## Compiler

### Windows

- MSVC (Visual Studio Build Tools 2022)

Standar:

- Full C++20 Support

---

## IDE / Editor

Supported:

- Visual Studio Code (utama)
- Visual Studio 2022
- CLion

Recommended:

- Visual Studio Code

Extensions:

- C/C++
- CMake Tools
- CMake
- GitLens

---

## Version Control

### Git

Repository:

- GitHub

Branch Strategy:

```text
main
 └── develop
      ├── feature/audio-engine
      ├── feature/overdrive
      └── feature/preset-manager
```

---

## Dependency Management

Untuk v0.1 dependency eksternal dijaga seminimal mungkin.

Allowed:

- JUCE
- Standard C++ Library

Future:

- Eigen (DSP Math)
- ONNX Runtime (AI Processing)
- RTNeural (Neural DSP)

---

## Audio Driver

### Windows

Primary:

- ASIO

Fallback:

- WASAPI

Target:

- Buffer Size 64–256 samples
- Sample Rate 44.1 kHz atau 48 kHz

---

## Coding Standards

### Naming Convention

Classes:

```cpp
AudioEngine
GainProcessor
PresetManager
```

Methods:

```cpp
prepareToPlay()
processBlock()
savePreset()
```

Variables:

```cpp
sampleRate
bufferSize
inputGain
```

Constants:

```cpp
constexpr float kMaxGain = 24.0f;
```

---

## Testing Requirements

Unit Testing:

- DSP Processor
- Preset Manager

Integration Testing:

- Audio Engine
- DSP Chain

Manual Testing:

- Realtime Audio Monitoring
- Device Switching
- Preset Loading

---

## Performance Requirements

CPU Usage:

- < 10% pada efek dasar (Gain + Overdrive + EQ)

Memory Usage:

- < 200 MB

Audio Latency:

- Target < 10 ms
- Ideal < 7 ms

---

## Logging & Debugging

Development Build:

- Console Logging Enabled

Release Build:

- Logging Minimal

Tools:

- JUCE Logger
- Visual Studio Debugger

---

## Future Technical Requirements

Fase berikutnya harus mempertimbangkan:

- Plugin Architecture (VST3/AU/LV2)
- Neural DSP Integration
- Tone Capture Engine
- Cross-platform Build Pipeline
- Dedicated Hardware Deployment

---

# 7. Hardware Requirements

## Development Machine

Minimum:

- Intel Core i5 Gen 4
- RAM 8 GB
- SSD
- Windows 11

Recommended:

- Intel Core i7 Gen 8+
- RAM 16 GB
- SSD NVMe

---

## Audio Interface

Supported:

- Focusrite Scarlett Series
- Audient EVO Series
- Behringer UMC Series

Requirement:

- ASIO Driver Support

---

# 8. Success Metrics

## Audio Performance

Target:

- Stable realtime audio processing
- No dropouts
- No clicks
- No pops

---

## Latency

Target:

- Less than 10 ms

Ideal:

- Less than 7 ms

---

## Stability

Application must:

- Run continuously for 1 hour
- Without crash
- Without memory leak
- Without audio interruption

---

# 9. System Architecture

```text
Audio Interface
        │
        ▼
Audio Device Manager
        │
        ▼
Audio Engine
        │
        ▼
DSP Chain
        │
        ├── Gain Processor
        ├── Overdrive Processor
        └── EQ Processor
        │
        ▼
Master Output
        │
        ▼
Speaker / Headphone
```

---

# 10. Core Modules

## Audio Module

Responsibilities:

- Audio Device Management
- Buffer Management
- Audio Callback Handling

Classes:

- AudioEngine
- AudioDeviceManager

---

## DSP Module

Responsibilities:

- Audio Processing
- Effect Chain Processing

Classes:

- AudioProcessorBase
- GainProcessor
- OverdriveProcessor
- EQProcessor

---

## Preset Module

Responsibilities:

- Save Preset
- Load Preset
- Delete Preset

Classes:

- PresetManager

---

## UI Module

Responsibilities:

- User Interface
- Parameter Control

Classes:

- MainWindow
- DevicePanel
- EffectPanel

---

# 11. Sprint Roadmap

## Sprint 0

### Hello Audio World

Goal:

Membuktikan audio realtime dapat berjalan.

Features:

- Device Detection
- Audio Input
- Audio Output
- Monitoring

DSP:

None

Flow:

```text
Input
 ↓
Output
```

Acceptance Criteria:

- Audio masuk
- Audio keluar
- Tidak crash
- Buffer Size terbaca
- Sample Rate terbaca

Output:

v0.0.1

---

## Sprint 1

### Audio Engine Foundation

Goal:

Membangun fondasi audio engine.

Features:

- Audio Engine
- Audio Processor Interface
- DSP Chain Manager

Output:

v0.1.0

---

## Sprint 2

### Clean Boost

Goal:

Membuat efek pertama.

Features:

- Gain Processor

Parameters:

- Gain

Output:

v0.2.0

---

## Sprint 3

### Overdrive

Goal:

Membuat distortion pertama.

Features:

- Soft Clipping

Parameters:

- Drive
- Level

Output:

v0.3.0

---

## Sprint 4

### Three Band EQ

Goal:

Membentuk karakter tone.

Features:

- Bass
- Mid
- Treble

Output:

v0.4.0

---

## Sprint 5

### Preset Management

Goal:

Menyimpan konfigurasi efek.

Features:

- Save Preset
- Load Preset
- Delete Preset

Format:

JSON

Output:

v0.5.0

---

# 12. Project Structure

```text
MilodikFX/

├── docs/
├── resources/
├── tests/

└── src/

    ├── audio/
    │   ├── AudioEngine
    │   └── AudioDeviceManager

    ├── dsp/
    │   ├── AudioProcessorBase
    │   ├── GainProcessor
    │   ├── OverdriveProcessor
    │   └── EQProcessor

    ├── preset/
    │   └── PresetManager

    └── ui/
        ├── MainWindow
        ├── DevicePanel
        └── EffectPanel
```

---

# 13. Long Term Roadmap

## Phase 2

- Noise Gate
- Compressor

## Phase 3

- Delay
- Chorus
- Flanger

## Phase 4

- Reverb

## Phase 5

- Cabinet IR Loader

## Phase 6

- Amp Simulator

Targets:

- British Amp
- American Amp
- Modern High Gain Amp

## Phase 7

- Plugin Support
- VST3
- AU
- LV2

## Phase 8

- macOS Support
- Linux Support

## Phase 9

- Tone Capture

## Phase 10

- AI Tone Assistant

## Phase 11

- Dedicated Hardware Platform

---

# Definition of Success

MilodikFX dianggap berhasil apabila:

1. Gitar dapat dimainkan realtime melalui audio interface.
2. Latensi nyaman digunakan untuk bermain gitar.
3. Audio bersih tanpa artefak.
4. Overdrive dan EQ terdengar musikal.
5. Preset dapat disimpan dan dipanggil kembali.
6. Menjadi fondasi yang kuat untuk membangun amp simulator profesional di masa depan.
