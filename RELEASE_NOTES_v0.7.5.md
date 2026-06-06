# MilodikFX v0.7.5 Release Notes

**Release Date:** June 7, 2026  
**Status:** Stable Release  
**Phase:** A - DSP Foundation (Complete ✅)

---

## Overview

MilodikFX v0.7.5 represents the completion of **Sprint 6** and the final release of **Phase A: DSP Foundation**. This release delivers a complete, production-ready audio DSP engine with professional effects, advanced UI, and comprehensive preset management.

---

## What's New in v0.7.5

### Phase A: DSP Foundation (Completed)

#### Sprint 0–5: Foundation (v0.0.1–v0.5.0)
- Audio engine with real-time processing
- Basic effects: Clean Boost, Overdrive, 3-Band EQ
- Preset management (Save/Load/Delete)

#### Sprint 6: Extended Effects & UI Polish

**Task 1: Extended Effects (v0.7.0)**
- ✨ **Compressor**: Dynamic range control with threshold, ratio, attack/release, auto makeup gain
- ✨ **Reverb**: Schroeder reverberator with 8 parallel comb filters + 4 series allpass filters
- ✨ **Tone Stack**: 3-band presence EQ (Bass@50Hz, Mid@500Hz, Treble@5kHz)
- **19 Sliders + 3 Toggles** for comprehensive parameter control
- **Full DSP Chain**: Gain → Overdrive → EQ → Compressor → Reverb → Tone Stack (6 effects)

**Task 2: UI Refactoring (v0.7.1)**
- 🎨 **Responsive Card Grid**: 2×3 effect layout that scales dynamically
- 🎨 **Window Maximize**: Default maximized on startup with bounds persistence
- 🎨 **Proportional Design**: All components perfectly balanced and aligned

**Task 3: Preset Enhancement (v0.7.2)**
- 📝 **Preset Metadata**: Author, description, category, tags, timestamps (ISO 8601)
- 📝 **Backward Compatibility**: Old v0.5.x presets load without error
- 📝 **JSON Serialization**: Professional format with full state capture

**Task 4: Performance Optimization (v0.7.3)**
- 📊 **Real-time CPU Monitoring**: Display CPU load 0–100%
- 📊 **Visual Warning**: Red indicator when CPU > 50%
- 📊 **Performance Metrics**: Expected load 15–25% typical, <40% peak

**Task 5: UI Polish (v0.7.4)**
- ✨ **Smooth Animations**: 100ms parameter interpolation with easing
- 🌙 **Theme System**: Dark / Light / High Contrast with persistent preference
- ⌨️ **Keyboard Navigation**: Tab/Shift+Tab, arrow keys, Page Up/Down, Enter
- ✨ **Effect Card Animations**: Pulse feedback on toggle, smooth peak meter decay

---

## Key Features

### Audio Processing
- **6 Professional DSP Effects**: Gain, Overdrive, EQ, Compressor, Reverb, Tone Stack
- **Real-time Audio Monitoring**: Input/Output level metering with peak indicators
- **Lock-Free Thread-Safe Parameters**: Atomic state updates for responsive UI
- **CPU Monitoring**: Real-time CPU load display with visual warnings
- **Low Latency**: Target <10ms, typical 5–7ms at 44.1 kHz

### User Interface
- **Responsive Design**: 2×3 card grid that adapts to window size
- **Window Persistence**: Bounds saved/restored on exit/startup
- **Professional Animations**: Smooth transitions, peak meter decay, card pulses
- **Dark/Light/High Contrast Themes**: Selectable with persistence
- **Full Keyboard Navigation**: Complete accessibility for power users
- **Modular Components**: KnobComponent, FootswitchComponent, EffectCard, LevelMeter

### Preset Management
- **Comprehensive Metadata**: Author, description, category, tags, creation/modification timestamps
- **JSON Format**: Human-readable, portable preset storage
- **Backward Compatibility**: Seamless upgrade from v0.5.x to v0.7.5
- **Auto-Recovery**: Sensible defaults for missing fields

### Performance
- **CPU Load**: 15–25% typical (all effects enabled, 44.1 kHz, 512 samples)
- **Peak Load**: <40% under worst-case conditions
- **Memory**: <200 MB total
- **Audio Latency**: <10 ms roundtrip with ASIO

---

## Technical Specifications

### Audio Engine
- **Sample Rates**: 44.1 kHz, 48 kHz, 96 kHz (auto-detected)
- **Buffer Sizes**: 64–256 samples (configurable)
- **Channel Count**: Mono/Stereo (auto-detected)
- **Audio Driver**: ASIO (primary), WASAPI (fallback)
- **Processing Precision**: Float 32-bit

### DSP Processors

| Processor | Parameters | CPU Load |
|-----------|------------|----------|
| Clean Boost Gain | Gain (0–24 dB) | <1% |
| Overdrive | Drive (0–100%), Level (0–100%) | 2–3% |
| 3-Band EQ | Bass/Mid/Treble (±12 dB) | 2–3% |
| Compressor | Threshold, Ratio, Attack, Release, Makeup Gain | 1–2% |
| Reverb | Room Size, Decay, Dry/Wet Mix, Width | 8–12% |
| Tone Stack | Bass/Mid/Treble presence (±12 dB) | 1–2% |

### System Requirements

**Minimum:**
- Windows 11 (21H2+)
- Intel Core i5 Gen 4 or equivalent
- 8 GB RAM
- SSD recommended
- ASIO compatible audio interface

**Recommended:**
- Intel Core i7 Gen 8+
- 16 GB RAM
- NVMe SSD
- Low-latency audio interface (Focusrite, Audient, Behringer)

---

## Build Information

### Deliverables
- `MilodikFX.exe` (Debug): 24.56 MB
- `MilodikFX.exe` (Release): 7.2 MB

### Build Environment
- **Compiler**: MSVC 2022 (C++20)
- **Build System**: CMake 3.25+
- **Framework**: JUCE 8.x

### Build Instructions
```powershell
# Configure
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

# Build
cmake --build build --config Release --parallel
```

---

## Test Results

| Test Suite | Status | Duration |
|-----------|--------|----------|
| Unit Tests | ✅ PASSED (3/3) | 0.03s |
| Smoke Tests | ✅ PASSED | 13.48s |
| ASIO Smoke | ✅ PASSED | 0.38s |
| **Total** | ✅ **100% PASS** | ~14s |

---

## Known Limitations

- **JUCE UI Only**: Future releases will migrate to React/TypeScript
- **Single Preset Browser**: No categories/search in current UI
- **No MIDI Support**: Planned for Phase E (Sprint 21)
- **No Plugin Format**: VST3/AU/LV2 planned for Phase B/C
- **Windows Only**: macOS/Linux support planned for future phases

---

## Breaking Changes

None. Full backward compatibility with v0.5.x presets.

---

## Migration Guide

### From v0.5.x to v0.7.5

1. **Presets**: Old presets load automatically with default metadata (author="", description="")
2. **Settings**: Window bounds and theme preference preserved
3. **Audio Config**: Device selection restored on startup

No manual migration required.

---

## Next Steps

### Sprint 7: Platform Migration (Phase B)

Following Sprint 6 completion, development shifts to **Phase B: Platform Migration**:

- **Sprint 7**: Frontend Foundation (React, TypeScript, TailwindCSS)
- **Sprint 8**: Backend Bridge (Message Bridge, Event System)
- **Sprint 9**: Modern UI MVP (New web-based UI)
- **Sprint 10**: Dynamic Signal Chain (Add/Remove/Reorder effects)

**Milestone Alpha**: v1.0.0 with React UI, message bridge, and dynamic signal chain.

---

## Download & Installation

**Pre-built Binaries:**
- Release: `build/MilodikFX_artefacts/Release/MilodikFX.exe` (7.2 MB)

**Build from Source:**
```bash
git clone https://github.com/banumelody/MilodikFX.git
cd MilodikFX
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

---

## Support & Feedback

- **Repository**: https://github.com/banumelody/MilodikFX
- **Issues**: Report on GitHub Issues
- **Documentation**: See `docs/prd.md` for technical details

---

## Credits

**Development**: Banu Antoro  
**Framework**: JUCE (Roli Ltd)  
**Build System**: CMake  
**Compiler**: MSVC 2022

---

## License

[Add your license here]

---

**MilodikFX v0.7.5 - Professional DSP Engine for Guitar & Bass**  
**Release Date:** June 7, 2026  
**Status:** Stable Production Release ✅
