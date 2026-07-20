# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

MilodikFX is a realtime guitar/bass multi-effect processor for Windows. It ships as a **single C++/JUCE executable** that runs the audio engine *and* an embedded HTTP server; the user-facing UI is a React app served by that server and opened in the default browser. There is no visible JUCE UI — `Main.cpp` creates a 1×1 hidden window and calls `juce::URL("http://localhost:3000").launchInDefaultBrowser()`.

## Build & run

Frontend **must** be built before the native build: `CMakeLists.txt` has a POST_BUILD step that copies `frontend/dist/{index.html,assets}` into `<exe dir>/resources/ui/web/`. `frontend/dist` is gitignored, so a fresh clone will fail the native build until the frontend is built.

```powershell
# 1. Frontend (produces frontend/dist)
cd frontend; npm ci; npm run build; cd ..

# 2. Native (JUCE 8.0.0 is fetched by CMake FetchContent on first configure — slow)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug --parallel

# 3. Run — opens http://localhost:3000 in the browser
build\MilodikFX_artefacts\Debug\MilodikFX.exe
```

Release exe: `cmake --build build --config Release --target MilodikFX --parallel` → `build\MilodikFX_artefacts\Release\MilodikFX.exe`.

Runtime log: `milodikfx.log` next to the executable. Startup is logged step-by-step there; it is the first place to look when the app appears to do nothing.

**Port 3000 is contended.** The Vite dev server (`frontend/vite.config.ts`) and the exe's `WebServer` both want 3000. Never run both. The exe falls back to 3001–3003 if 3000 is taken, which silently breaks the frontend since `audioApi.ts` hardcodes `http://localhost:3000/api`.

ASIO is optional and off unless the Steinberg SDK is found. Set `$env:ASIOSDK_DIR` (or `ASIO_SDK_PATH`) before configuring and CMake auto-enables `MILODIKFX_ENABLE_ASIO`.

## Tests

Backend (JUCE `UnitTest` classes aggregated into one binary — `tests/UnitTests.cpp`):

```powershell
cmake --build build --config Debug --target MilodikFX_tests
ctest --test-dir build -C Debug --output-on-failure          # all: unit + smoke + smoke-asio
ctest --test-dir build -C Debug -R MilodikFX_UnitTests --output-on-failure
build\MilodikFX_tests_artefacts\Debug\MilodikFX_tests.exe    # direct, prints per-test output
```

There is **no per-test filter** — `runAllTests()` runs every statically-registered `UnitTest`. To iterate on one, temporarily comment out the other `static XTests xTests;` registrations at the bottom of each class. `MilodikFX_Smoke_ASIO` skips when ASIO is disabled or no driver is present.

Frontend (from `frontend/`):

```powershell
npm run test -- --run           # vitest, headless (watch mode is the default without --run)
npm run test -- -t "pattern"    # single test/suite by name
npm run lint                    # eslint src --ext .ts,.tsx
npm run type-check              # tsc --noEmit (also runs as part of npm run build)
npm run e2e:headless            # Cypress; expects a server on http://localhost:5173
```

Cypress `baseUrl` is **5173**, not 3000 — E2E runs against `npx vite preview --port 5173`, not against the exe. Only `PerformViewSimplified.tsx` carries `data-testid`s, and it exposes `window.__setMasterVolume` specifically for E2E.

End-to-end against the real exe: `.github/scripts/run-local-e2e.ps1 [-Build]` starts the exe, waits for `/index.html`, and smoke-checks `/api/levels` and GET/PUT `/api/parameters/master-volume`.

CI (`.github/workflows/ci.yml`) is **frontend-only on ubuntu-latest** — the native build is never exercised in CI. C++ changes are verified locally or not at all.

## Architecture

### Request path

`WebServer` (`src/ui/WebServer.cpp`) is a raw Winsock2 server on a `juce::Thread`, one detached `std::thread` per connection. Paths under `/api` go to `RestApiDispatcher`, everything else is served as a static file from `<exe dir>/resources/ui/web/`.

`RestApiDispatcher` does **longest-prefix** matching over registered path prefixes, then dispatches by method to an `HttpHandler` subclass (`src/api/*Handler.*`). Handlers return `{statusCode, contentType, body}`. Registration happens in the `MainComponent` constructor (`/api/devices`, `/api/parameters`, `/api/effects`, `/api/levels`, `/api/presets`, `/api/health`).

Handlers do their own sub-path parsing with `std::string::find`, and mostly **build and parse JSON by hand with string concatenation**. `PresetsHandler` is the exception and the direction to move in — it parses request bodies with `juce::JSON::parse` into a `juce::var`. Responses are still assembled as raw strings everywhere, so any value interpolated into one (preset names in particular) is unescaped.

### Audio path

`MainComponent` is the composition root and owns everything. Its constructor order is deliberate and load-bearing:

1. `WebServer` created and started **first** (so the UI is reachable even if audio hangs),
2. DSP processors constructed and pushed into `audioEngine.getChain()`,
3. REST handlers constructed over those objects and registered,
4. audio device initialised on a **detached background thread** (`initialiseWithDefaultDevices`) — it can block for seconds on bad drivers,
5. settings loaded from `PropertiesFile` and pushed into the processors.

Signal chain, in fixed order: `GainProcessor` (clean boost) → `OverdriveProcessor` → `EQProcessor` → `CompressorProcessor` → `ReverbProcessor` → `ToneStackProcessor`. Each derives from `milodikfx::dsp::AudioProcessorBase` (`prepareToPlay`/`processBlock`/`reset`) and holds its parameters as `std::atomic` so the REST thread can write while the audio thread reads.

`DSPChainManager::findProcessor<T>()` is a `dynamic_cast` scan that returns the **first** instance of a type. Every REST handler reaches its target this way, so **the chain may contain at most one processor of each type** — adding a second overdrive would silently make it unaddressable via the API. Changing that means giving processors IDs.

### Persistence

- Settings: `%APPDATA%\MilodikFX\MilodikFX.settings` (`juce::PropertiesFile`, XML). Keys follow `dsp.<effect>.<param>` (e.g. `dsp.overdrive.drivePct`) and are declared as `static constexpr` members at the top of `MainComponent.h`. Writes are debounced — `markSettingsDirty()` + a 1 s timer with a 2 s minimum interval.
- Presets: JSON under `Documents\MilodikFX\Presets`, via `milodikfx::preset::PresetManager` and the flat `PresetState` struct.

Adding a parameter therefore touches: the processor, `MainComponent.h` key constant, load + `saveSettingsIfNeeded` in `MainComponent.cpp`, `PresetState`/`PresetManager` if it should be preset-able, `ParametersHandler`/`EffectsHandler`, and `frontend/src/services/audioApi.ts`.

### Frontend

`main.tsx` → `App.tsx` → `PerformViewSimplified.tsx`, which uses `RotaryKnob.tsx` and `services/audioApi.ts` and nothing else. It fetches devices on mount and polls `/api/levels` at 100 ms via `subscribeLevels`.

Namespaces: `milodikfx::audio`, `milodikfx::dsp`, `milodikfx::preset` for engine code; API handlers and `WebServer` are in the global namespace.

## Dormant code — do not assume it runs

Large parts of the tree are abandoned or half-built experiments from earlier sprints. Check whether something is reachable before modifying it:

- `src/ui/*Component.{h,cpp}` (KnobComponent, EffectCardComponent, LevelMeterComponent, …) — the old native JUCE UI. **Not in `CMakeLists.txt`; not compiled.** Only `src/ui/WebServer.*` is live.
- `src/ipc/*` (IPCServer, MessageHandler, TunerBridge, MetronomeBridge) — compiled into the exe but **never instantiated**; `MainComponent` doesn't include them.
- `electron/`, `binding.gyp`, `src/native/*`, and the root `package.json` scripts (`npm run dev`, `dist:win`) — an Electron shell for a planned v0.9. `electron/main.js` hardcodes `isDev = false`, the native addon is a `HelloWorld` stub that is never `require`d, and its meter data is `Math.random()`. Not the shipping path.
- Most of `frontend/src/components/` (Dashboard, DashboardV2, PerformView, MainLayout, EffectCard, …), `frontend/src/hooks/`, and `frontend/src/services/{audioEngine,messageBridge,effectManager,eventDispatcher}.ts` — orphaned; nothing imports them from `App.tsx`. The socket.io dependency and `types/ipc.ts` belong to this dead path.

Known gap: `DevicesHandler` reads `deviceManager.getAudioDeviceSetup()`, which comes back empty even when a device is open — `GET /api/devices` reports `sampleRate: 0`, `bufferSize: 0` and `"Default"` for both device names while the log shows the device actually running at 48 kHz with a 480-sample block. Read the live device via `getCurrentAudioDevice()` instead.

When debugging meters or device state, `milodikfx.log` now records the device name, sample rate, block size and channel counts at `audioDeviceAboutToStart`. Note that a silent input legitimately reads near the `kMeterFloorDb` floor (-100 dBFS); a real interface idles around -78 dBFS, so anything pinned at exactly the floor means no blocks are arriving.

## Docs

`README.md` and the many `SPRINT_*` / `RELEASE_*` / `TEST_*` markdown files at the repo root are historical sprint reports (partly in Indonesian) and describe states the code has since moved past — treat them as changelog, not spec. `docs/prd.md` holds the product requirements and naming conventions. `.github/copilot-instructions.md` is the closest thing to a maintained command reference.
