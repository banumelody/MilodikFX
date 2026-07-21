# MilodikFX

MilodikFX adalah aplikasi multi-effect gitar/bass berbasis DSP realtime (C++20 + JUCE) untuk Windows 11.

Status **v0.7.5 (Sprint 6 Complete):**
- **DSP Chain:** Clean Boost → Overdrive → 3-Band EQ → Compressor → Reverb → Tone Stack (6 effects)
- **Advanced Features:**
  - Real-time CPU load monitoring (0–100%, visual warning @ 50%+)
  - Smooth animations for parameter changes (100ms interpolation)
  - Dark/Light/High Contrast themes with persistent preference
  - Keyboard navigation (Tab/Shift+Tab, arrows, Page Up/Down)
  - Comprehensive preset metadata (author, description, tags, timestamps)
- **UI/UX Polished:**
  - Proportional effect cards in responsive 2×3 grid
  - Window maximized on startup with bounds persistence
  - Smooth level meter animations with peak decay
  - Modular UI components (Knob, Footswitch, EffectCard)
- **Audio Performance:**
  - Typical CPU load: 15–25% (all effects enabled at 44.1 kHz)
  - Peak load <40% (includes reverb @ 8–12%)
  - Lock-free thread-safe parameter updates
- **Preset Management:** Save/Load/Delete with JSON, full backward compatibility

## Build (Windows)
Prerequisites:
- CMake 3.25+
- Visual Studio 2022 Build Tools (MSVC)

Configure + build:
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug --parallel
```

Run (Debug):
- `build\MilodikFX_artefacts\Debug\MilodikFX.exe`

Tests (Debug):
```powershell
cmake --build build --config Debug --target MilodikFX_tests
ctest --test-dir build -C Debug --output-on-failure
```
Catatan: `MilodikFX_Smoke_ASIO` akan skip jika build tidak mengaktifkan ASIO atau driver ASIO tidak terdeteksi. Jika driver ada dan ASIO aktif tapi tidak terpilih, test akan gagal.

ASIO (opsional):
- Install Steinberg ASIO SDK (local)
- Set env `ASIOSDK_DIR` atau `ASIO_SDK_PATH`, atau isi `MILODIKFX_ASIO_SDK_PATH` di CMake
- Jika SDK terdeteksi valid, `MILODIKFX_ENABLE_ASIO` akan auto ON

```powershell
# via env var (recommended)
$env:ASIOSDK_DIR = "C:\\path\\to\\asiosdk"
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

# or explicit path
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DMILODIKFX_ASIO_SDK_PATH="C:\\path\\to\\asiosdk"

# manual override (if needed)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DMILODIKFX_ENABLE_ASIO=ON
```

## Licence

MilodikFX is licensed under the **GNU Affero General Public License v3.0** — see [LICENSE](LICENSE).

That choice is not arbitrary. The binaries link the JUCE framework, whose modules are dual-licensed
under AGPLv3 or a commercial JUCE licence; distributing a build without the commercial licence means
the combined work is AGPLv3. Builds that also enable ASIO include Steinberg's ASIO SDK, offered under
either a signed Steinberg agreement or GPLv3 — GPLv3 section 13 explicitly permits combining with
AGPLv3 code, so the AGPLv3 release covers that too.

Practically: you may use, modify and redistribute this, including commercially, provided you pass on
the same freedoms and make the corresponding source available. The full source is in this repository.

Third-party components:

- [JUCE](https://juce.com) — AGPLv3 / commercial, © Raw Material Software Limited
- [Steinberg ASIO SDK](https://www.steinberg.net/developers/) — ASIO is a trademark and software of
  Steinberg Media Technologies GmbH. Only present in builds made with `MILODIKFX_ENABLE_ASIO=ON`;
  the SDK itself is not redistributed in this repository.
- [Microsoft Edge WebView2](https://developer.microsoft.com/microsoft-edge/webview2/) — used at
  runtime to render the interface.
