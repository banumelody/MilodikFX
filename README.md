# MilodikFX

MilodikFX adalah aplikasi multi-effect gitar/bass berbasis DSP realtime (C++20 + JUCE) untuk Windows 11.

Status v0.5.5:
- Monitoring passthrough + input/output metering
- DSP Chain: Clean Boost (0–24 dB) → Overdrive (Drive/Level %) → 3 Band EQ (Bass/Mid/Treble ±12 dB)
- Preset management: Save / Load / Delete (JSON)
- **UI/UX refinement**: Proportional knobs, horizontal EQ layout, improved footswitch buttons
- **Footswitch proportions**: 56px buttons with proper spacing and label positioning
- **Window sizing**: Default 1200×700 optimized for 1920×1080 (Full HD) laptop screens

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
