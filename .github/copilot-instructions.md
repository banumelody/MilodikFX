# Copilot instructions for MilodikFX

## Build, test, lint
### Build (Windows, CMake)
- Configure + build (Debug):
  ```powershell
  cmake -S . -B build -G "Visual Studio 17 2022" -A x64
  cmake --build build --config Debug --parallel
  ```
- JUCE is pulled via CMake `FetchContent` (see root `CMakeLists.txt`).

### ASIO (optional)
- ASIO is disabled by default (requires Steinberg ASIO SDK locally).
  ```powershell
  cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
    -DMILODIKFX_ENABLE_ASIO=ON `
    -DMILODIKFX_ASIO_SDK_PATH="C:\\path\\to\\asiosdk"
  ```

### Tests / lint
- **Frontend:** Vitest + React Testing Library (40+ unit tests, 30+ E2E scenarios with Cypress)
- **Backend:** JUCE native compilation (CMake integrated)
- Both fully automated in build pipeline

## Frontend Stack (Sprint 7.5 - Current)
- **React 18.2.0** - Component-based modern UI
- **TypeScript 5.1.3** - Strict type safety
- **TailwindCSS 3.3.2** - Utility-first CSS
- **Vite 4.5.14** - Fast bundler & dev server
- **Vitest + React Testing Library** - Testing framework

## Backend Architecture (Sprint 7.5 - Current)
- **C++ REST API Server** - WebServer class serving React on :3000
- **JUCE 8.0.0** - Audio processing only (no UI components visible to user)
- **Hidden JUCE Window** - 1x1 pixel invisible container for JUCE infrastructure
- **Auto-launch Browser** - Automatically opens React UI on http://localhost:3000

## Communication Protocol
- **HTTP REST API** - React frontend calls C++ backend endpoints
- **JSON Serialization** - All messages use JSON format
- **Port:** localhost:3000 (WebServer)

## Build Artifacts
- **Windows Executable:** MilodikFX.exe (~6.9 MB Release build)
  - Contains: C++ audio engine + REST API server + embedded React frontend
  - Single-file distribution (ready for production release)
- **Frontend Assets:** Bundled in resources/ui/web/ directory
  - index.html, bundle.js, styles.css (production-optimized)

## High-level architecture
- Target pipeline (from PRD): Audio Interface → Audio Device Manager → Audio Engine → DSP Chain (Gain, Overdrive, EQ, Compressor, Reverb, ToneStack, NoiseGate) → Master Output.
- Current implementation (Sprint 7.5): **HYBRID ARCHITECTURE**
  - **Backend (C++ only):** `MainComponent` (hidden) owns `juce::AudioDeviceManager` + `AudioIODeviceCallback` for audio processing; `WebServer` serves React frontend on localhost:3000
  - **Frontend (HTML/JS only):** React 18 + TypeScript 5 + TailwindCSS 3 for all user UI (no C++ components visible to user)
  - **IPC Communication:** C++ REST API endpoints serve React requests for device management, parameter control, effect chain, presets
- Core modules: `src/audio/` (engine), `src/dsp/` (processors), `src/preset/` (management), `src/ui/WebServer.h/cpp` (REST API server)

## Key conventions
- Naming (PRD): classes `AudioEngine`, `GainProcessor`, `PresetManager`; methods `prepareToPlay()`, `processBlock()`, `savePreset()`; variables `sampleRate`, `bufferSize`, `inputGain`; constants `constexpr float kMaxGain = 24.0f;`.
- Keep dependencies minimal in v0.1: JUCE plus the C++ standard library only.
- Presets are planned to be stored as JSON.
