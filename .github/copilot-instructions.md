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
- No test runner/lint tooling is wired up yet in the repository.

## High-level architecture
- Target pipeline (from PRD): Audio Interface → Audio Device Manager → Audio Engine → DSP Chain (Gain, Overdrive, EQ) → Master Output.
- Current implementation (Sprint 0): `MainComponent` owns a `juce::AudioDeviceManager` + `AudioIODeviceCallback` and performs input→output passthrough plus input level metering; UI embeds `juce::AudioDeviceSelectorComponent`.
- Core modules planned by PRD: `src/audio`, `src/dsp`, `src/preset`, `src/ui`.

## Key conventions
- Naming (PRD): classes `AudioEngine`, `GainProcessor`, `PresetManager`; methods `prepareToPlay()`, `processBlock()`, `savePreset()`; variables `sampleRate`, `bufferSize`, `inputGain`; constants `constexpr float kMaxGain = 24.0f;`.
- Keep dependencies minimal in v0.1: JUCE plus the C++ standard library only.
- Presets are planned to be stored as JSON.
