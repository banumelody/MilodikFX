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

Setelah evaluasi Sprint 6, diputuskan bahwa MilodikFX akan melakukan migrasi arsitektur UI dari JUCE Native Components menuju arsitektur Hybrid Frontend menggunakan React, TypeScript, dan TailwindCSS.

Keputusan ini diambil untuk:
- Mempercepat pengembangan UI
- Mendukung desain modern
- Mendukung signal chain visual
- Mendukung drag & drop routing
- Mendukung mobile/web companion di masa depan
- Memisahkan DSP Engine dan UI Layer

Mulai Sprint 7, fokus utama bukan lagi penambahan efek baru, tetapi pembangunan fondasi platform generasi berikutnya. DSP Foundation (Phase A) telah selesai di Sprint 6, dan sekarang akan dimulai Platform Migration (Phase B) dengan arsitektur modern berbasis web.

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

### JUCE (Phase A & DSP Engine)

Versi minimum:

- JUCE 8.x atau versi stabil terbaru

JUCE digunakan untuk:

- Audio Device Management
- Audio Callback
- DSP Utilities
- DSP Processor Implementation
- Cross-platform Support
- Plugin Framework (fase berikutnya)

Alasan pemilihan:

- Standar industri audio software
- Digunakan oleh banyak produk profesional
- Dokumentasi dan komunitas kuat
- Mempermudah migrasi ke VST3, AU, dan standalone application

---

### Frontend Stack (Phase B onwards - UI Layer)

#### React

Versi minimum:

- React 18.x

Digunakan untuk:

- Modern UI Development
- Component-based Architecture
- State Management
- Event Handling

#### TypeScript

Versi minimum:

- TypeScript 5.x

Digunakan untuk:

- Type Safety
- Better Developer Experience
- Maintainability
- Code Quality

#### TailwindCSS

Versi minimum:

- Tailwind CSS 3.x

Digunakan untuk:

- Responsive Design System
- Design Tokens
- Theme Management
- Rapid UI Development

---

## Desktop Integration (Phase B onwards)

### Electron (Optional, for future desktop app)

- Desktop App wrapper
- Native OS integration
- Main Process ↔ Renderer Process communication
- IPC to C++ DSP Engine

Alasan: Memungkinkan deployment cross-platform (Windows, macOS, Linux)

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

Roadmap dibagi menjadi enam fase utama: DSP Foundation (Phase A, Completed), Platform Migration (Phase B, In Progress), Professional DSP Effects (Phase C), Amp Platform (Phase D), Milodik Ecosystem (Phase E), dan Intelligent Processing (Phase F).

## Phase A: DSP Foundation (Completed ✓)

Menbangun fondasi DSP Engine yang stabil dengan efek dasar dan preset management.

### Sprint 0 - Hello Audio World
**Output:** v0.0.1  
**Deliverables:** Device Detection, Audio Input/Output, Monitoring

### Sprint 1 - Audio Engine Foundation
**Output:** v0.1.0  
**Deliverables:** Audio Engine, Audio Processor Interface, DSP Chain Manager

### Sprint 2 - Clean Boost
**Output:** v0.2.0  
**Deliverables:** Gain Processor (0–24 dB)

### Sprint 3 - Overdrive
**Output:** v0.3.0  
**Deliverables:** Soft Clipping Processor, Drive Control, Level Control

### Sprint 4 - Three Band EQ
**Output:** v0.4.0  
**Deliverables:** Bass (±12 dB @ 50Hz), Mid (±12 dB @ 500Hz), Treble (±12 dB @ 5kHz)

### Sprint 5 - Preset Management
**Output:** v0.5.0  
**Deliverables:** Save, Load, Delete presets (JSON format)

### Sprint 6 - Extended Effects & UI Polish
**Output:** v0.7.5  
**Deliverables:**
- 3 New Processors: Compressor, Reverb, Tone Stack
- UI Refactoring: Responsive 2×3 card grid, window maximize
- Preset Metadata: Author, description, tags, timestamps (ISO 8601)
- Performance Optimization: Real-time CPU monitoring (0–100%, warning @ 50%)
- UI Polish: Smooth animations (100ms), Dark/Light/High Contrast themes, keyboard navigation (Tab/arrows)

**Status:** Phase A Complete ✓

---

## Phase B: Platform Migration (In Progress)

Memigrasikan UI menjadi arsitektur modern berbasis web dengan React, TypeScript, TailwindCSS.

### Sprint 7 - Frontend Foundation
**Target Output:** v0.8.0  
**Goal:** Membangun fondasi frontend modern  
**Deliverables:**
- React Setup (Vite)
- TypeScript Configuration
- TailwindCSS Integration
- Design Tokens & System
- Dark/Light Theme Foundation
- Component Library Setup
- Frontend Build Pipeline

### Sprint 8 - Backend Bridge
**Target Output:** v0.9.0  
**Goal:** Membangun komunikasi antara frontend dan DSP Engine  
**Deliverables:**
- Message Bridge (IPC/WebSocket)
- Event System Architecture
- Parameter Synchronization Protocol
- Device State API
- Preset API (CRUD operations)

### Sprint 9 - Modern UI MVP
**Target Output:** v1.0.0-beta  
**Goal:** Implementasi UI generasi baru  
**Deliverables:**
- Preset Bar (Browser + Save/Load UI)
- Device Panel (I/O Selection, Latency Monitor)
- Audio Meter (Stereo Input/Output + Peak Indicators)
- Effect Cards (Visual representation of DSP chain)
- Status Bar (CPU Load, Sample Rate, Buffer Size)
- Complete Theme System

### Sprint 10 - Dynamic Signal Chain
**Target Output:** v1.0.0 (Release Candidate)  
**Goal:** Signal chain tidak lagi fixed, mendukung Add/Remove/Reorder  
**Deliverables:**
- Add Effect UI
- Remove Effect UI
- Reorder Effect (Drag & Drop)
- Signal Routing Visualization
- Effect Enable/Disable Toggles
- Audio Path Animation

**Milestone Alpha:** React UI + TailwindCSS + Message Bridge + DSP Engine + Dynamic Signal Chain

---

## Phase C: Professional DSP Effects

Menambahkan efek profesional di atas fondasi baru.

### Sprint 11 - Compressor
**Output:** v1.1.0  
**Deliverables:** Threshold, Ratio, Attack, Release parameters

### Sprint 12 - Delay
**Output:** v1.2.0  
**Deliverables:** Time, Feedback, Mix parameters

### Sprint 13 - Reverb Enhancement
**Output:** v1.3.0  
**Deliverables:** Decay, Mix, Room Size refinement

### Sprint 14 - Chorus
**Output:** v1.4.0  
**Deliverables:** Rate, Depth, Mix parameters

### Sprint 15 - Flanger
**Output:** v1.5.0  
**Deliverables:** Rate, Feedback, Mix parameters

### Sprint 16 - Cabinet IR Loader
**Output:** v1.6.0  
**Deliverables:** WAV Loader, IR Manager, Convolution Engine

---

## Phase D: Amp Platform

Membangun platform amp simulator profesional.

### Sprint 17 - Cabinet Manager
**Output:** v1.7.0  
**Deliverables:** Cabinet Browser, Categories, Favorites system

### Sprint 18 - British Amp
**Output:** v1.8.0  
**Inspired by:** Marshall Style  
**Deliverables:** Preamp, Tone Stack, Power Amp simulation

### Sprint 19 - American Amp
**Output:** v1.9.0  
**Inspired by:** Fender Style

### Sprint 20 - Modern High Gain Amp
**Output:** v2.0.0  
**Inspired by:** 5150, Mesa Style

---

## Phase E: Milodik Ecosystem

Membangun ekosistem penggunaan live dan studio.

### Sprint 21 - MIDI Support
**Output:** v2.1.0  
**Deliverables:** MIDI Learn, MIDI Mapping

### Sprint 22 - Footswitch Support
**Output:** v2.2.0  
**Deliverables:** Scene Switching, Preset Switching via hardware

### Sprint 23 - Performance Mode
**Output:** v2.3.0  
**Deliverables:** Live View, Fullscreen Mode, Setlist Mode

### Sprint 24 - Preset Marketplace Foundation
**Output:** v2.4.0  
**Deliverables:** Preset Categories, Local Library, Marketplace Architecture

### Sprint 25 - Remote Browser Control
**Output:** v2.5.0  
**Deliverables:** Browser UI, Tablet UI, Mobile UI companion

---

## Phase F: Intelligent Processing

Menjadikan MilodikFX platform generasi berikutnya.

### Sprint 26 - Tone Capture Research
**Output:** Research Release  
**Deliverables:** Capture Architecture, Benchmark Framework

### Sprint 27 - AI Preset Recommendation
**Output:** v2.7.0  
**Deliverables:** Genre Recommendation, Preset Suggestion engine

### Sprint 28 - AI Tone Assistant
**Output:** v3.0.0  
**Deliverables:** Tone Assistant, Auto EQ Suggestion, Signal Chain Recommendation

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

# 13. Long Term Vision

MilodikFX akan dibangun sebagai platform modular dengan DSP Engine sebagai core yang dapat diintegrasikan ke berbagai platform:

```text
Milodik Engine (C++20 DSP Core)
        │
        ├── Desktop App (React UI + Hybrid)
        ├── Plugin (VST3/AU/LV2)
        ├── Hardware Platform
        ├── Web Editor
        └── Mobile Companion
```

Dengan prinsip arsitektur:

- **DSP Engine** adalah produk utama (C++20, JUCE, Audio Processing)
- **UI Layer** adalah client independen (React, TypeScript, TailwindCSS)
- **Audio Processing** tetap 100% berjalan di C++ real-time thread
- **Frontend** dapat berkembang secara independen tanpa mempengaruhi DSP
- **Interoperability** antar platform (desktop, plugin, web, mobile)
- **Siap untuk** future hardware deployment, plugin ecosystem, dan AI integration

---

# 14. Definition of Success

MilodikFX dianggap berhasil apabila:

1. **Phase A (DSP Foundation) ✓** - Memiliki DSP Engine yang stabil dan modular dengan 6+ efek profesional
2. **Phase B (Platform Migration)** - Memiliki UI modern berbasis React yang mudah dikembangkan dan di-maintain
3. **Dynamic Signal Chain** - Mendukung penambahan/penghapusan/pengurutan efek secara dinamis
4. **Professional Processing** - Mampu menjalankan efek profesional dengan latency <10ms dan CPU load <40%
5. **Scalable Foundation** - Menjadi fondasi yang kuat untuk amp simulator, tone capture, AI processing, dan hardware
6. **Cross-Platform Ready** - Dapat berkembang menjadi platform audio profesional lintas perangkat (desktop, plugin, web, mobile)
