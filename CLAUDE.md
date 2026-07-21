# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

MilodikFX is a realtime guitar/bass multi-effect processor for Windows, built for one person's own rig.

It ships as a **single self-contained C++/JUCE executable**. The exe runs the audio engine, serves a
loopback-only HTTP API, and hosts the React UI *inside its own window* via Edge WebView2. There is no
browser tab and no separate UI process. The UI bundle is embedded in the exe, so the binary runs on its
own with nothing beside it.

A second target builds the same DSP chain as a **VST3 plugin** plus a JUCE Standalone wrapper.

## Build & run

The frontend must be built **before** CMake configures, because the bundle is embedded into the exe at
configure time.

```powershell
# Everything, including the installer if Inno Setup is present:
powershell -ExecutionPolicy Bypass -File scripts\build-release.ps1

# ...or by hand:
cd frontend; npm ci; npm run build; cd ..
cmake -S . -B build -G "Visual Studio 17 2022" -A x64      # JUCE + WebView2 are fetched on first run
cmake --build build --config Release --parallel
build\MilodikFX_artefacts\Release\MilodikFX.exe
```

If `frontend/dist` is missing, CMake warns and the exe falls back to serving from
`<exe dir>/resources/ui/web`, then to a built-in "UI not built" page. **Re-run CMake after the first
frontend build** or the embedded copy will be stale.

Runtime log and settings live in `%APPDATA%\MilodikFX\` (`milodikfx.log`, `MilodikFX.settings`).
Presets are JSON under `Documents\MilodikFX\Presets`.

**Ports.** The engine binds `127.0.0.1:3000`, falling back through 3008. The Vite dev server is on
**5173** so the two can never contend. The UI resolves its API base from `window.location.origin`, so
the port fallback is handled automatically.

**ASIO** is optional. Set `$env:ASIOSDK_DIR` (or pass `-AsioSdkPath` to the build script) before
configuring and CMake enables it. Without the SDK the engine still reaches low latency through WASAPI
exclusive mode.

## Tests

```powershell
# Backend: JUCE UnitTest classes aggregated into one binary
cmake --build build --config Debug --target MilodikFX_tests
build\MilodikFX_tests_artefacts\Debug\MilodikFX_tests.exe     # prints per-test output to stdout
ctest --test-dir build -C Debug --output-on-failure           # unit tests + HTTP smoke test

# Frontend
cd frontend
npx vitest run              # unit tests (note: `npm run test` starts watch mode)
npm run type-check
npm run lint

# End-to-end against a real running engine
powershell -ExecutionPolicy Bypass -File .github\scripts\run-local-e2e.ps1 [-Build]
```

There is **no per-test filter** in the native suite — `runAllTests()` runs every statically-registered
`UnitTest`. To iterate on one, comment out the other `static XTests xTests;` registrations.

`tests/smoke.ps1` starts the exe, probes ports 3000-3008, and exercises the HTTP surface (metering,
the effect list, a parameter round-trip, clamping, unknown-parameter rejection, path-traversal
defence). It backs up and restores the user's settings file rather than deleting it.

CI (`.github/workflows/ci.yml`) is **frontend-only on ubuntu-latest** — the native build is never
exercised there. C++ changes are verified locally or not at all.

## Architecture

### Composition root

`MainComponent` owns everything and is also the window's content component. Construction order is
load-bearing:

1. DSP chain built (`buildGuitarChain`), parameter registry populated, settings applied,
2. HTTP server started and handlers registered,
3. WebView created and pointed at the local server,
4. audio device opened from a **deferred message-thread callback** — it can block for seconds on a bad
   driver, and a `Component::SafePointer` means quitting during startup cannot touch a dead object.

### The parameter registry is the spine

`milodikfx::api::ParameterRegistry` (`src/api/ParameterRegistry.*`) holds one descriptor per effect and
parameter: id, label, unit, range, step, default, and getter/setter closures over the live processor.

Everything reads from it: the REST API, the UI (which builds itself from `GET /api/effects`), preset
capture/restore, and settings persistence (`dsp.<effectId>.<parameterId>`). **Adding a parameter means
editing `src/dsp/ChainFactory.cpp` and nothing else** — it appears in the API, the UI, presets and the
settings file automatically.

Ids are camelCase so they double as settings keys; lookups are case-insensitive so URLs need not care.

### Signal chain

Built by `milodikfx::dsp::buildGuitarChain` (`src/dsp/ChainFactory.*`), shared by the app and the
plugin, in this fixed order:

```
NoiseGate -> CleanBoost -> Compressor -> Overdrive -> EQ -> Contour -> Cabinet -> Delay -> Reverb -> MasterOut
```

Each processor derives from `AudioProcessorBase` (`prepareToPlay`/`processBlock`/`reset`) and holds its
parameters as `std::atomic`. `MasterOut` is the only stage that can attenuate, and it carries the
safety limiter plus a final clamp — **nothing may be added after it**.

`DSPChainManager::findProcessor<T>()` is a `dynamic_cast` scan returning the *first* instance of a
type, so the chain may contain at most one processor of each type.

### Realtime rules

These are not stylistic; each corresponds to a bug that shipped:

- **No allocation in `processBlock`.** Filter coefficients are plain `BiquadCoeffs` (`src/dsp/Biquad.h`)
  recomputed on the audio thread only when a smoothed value actually moved. The old code built
  `juce::dsp::IIR::Coefficients` objects on the audio thread on every knob turn.
- **No coefficients shared across threads.** REST threads write atomics; the audio thread derives
  everything else. Three processors used to race plain floats.
- **Rebuild after `prepareToPlay`.** Fresh filters must be given coefficients explicitly; the EQ once
  synced its change-detection cache before updating, so it went silently flat after every device restart.
- **Guard every envelope.** `std::isfinite` checks and clamps in the compressor, limiter, gate and
  delay. A divide-by-zero in the compressor's gain computer used to latch NaN into the signal
  permanently.
- `ScopedNoDenormals` in the audio callback; parameter smoothing on every gain.

### Audio device

`milodikfx::audio::AudioDeviceController` (`src/audio/AudioDeviceController.*`) owns **every**
interaction with `juce::AudioDeviceManager` and marshals each one onto the message thread. REST handlers
run on arbitrary Winsock threads and must never touch the device manager directly.

Device selection walks preferred types: ASIO -> WASAPI Exclusive -> WASAPI Low Latency -> WASAPI shared
-> DirectSound. Three things it gets right that are easy to break again:

- `initialise(2, 2, ...)` is what tells the manager how many input channels are *needed*. Calling only
  `setAudioDeviceSetup` leaves that at zero and opens the device output-only.
- The saved state is only restored when it is a real `DEVICESETUP` element. The manager silently ignores
  an unrecognised one, opens the default device, and reports no error — which looks exactly like success.
- Buffer/rate **preferences** change only when the user asks. Adopting whatever the device happened to
  open at once made the app treat a 2048-sample fallback as its target forever after.

On the developer's Scarlett this reaches 288 samples at 96 kHz, 12 ms round trip, ~2.4 % CPU in Release.

### HTTP layer

`WebServer` (`src/ui/WebServer.cpp`) is a raw Winsock2 server on a `juce::Thread`, one detached
`std::thread` per connection. It binds **loopback only** — the endpoint can switch audio hardware and
write files. `stop()` waits for in-flight connections, because handlers hold references to objects the
owner destroys immediately afterwards.

`RestApiDispatcher` does longest-prefix matching, then dispatches by method to an `HttpHandler`
subclass. Responses are built with `juce::var` + `juce::JSON` (see `src/api/ApiJson.h`) — never by
string concatenation.

Endpoints: `/api/effects`, `/api/parameters`, `/api/devices`, `/api/levels`, `/api/presets`,
`/api/health`.

### Frontend

`main.tsx` -> `App.tsx` -> `components/{EffectRack, Knob, Toggle, LevelMeter, DeviceSettings, PresetControls}`
plus `services/api.ts`. Nothing else exists; the dormant component tree was deleted.

The UI is generated from the registry, so it cannot drift from the engine. Knob interaction is a
**relative vertical drag** from the press point (shift = fine, wheel, double-click to default, full
keyboard). Parameter writes are coalesced per parameter on a 40 ms timer so a drag cannot flood the
thread-per-connection server.

Vite emits **stable filenames** (`assets/index.js`, `assets/index.css`) — the embedding step and the
resources copy both depend on that.

## Gotchas

- `npm run test` starts vitest in watch mode and will hang a scripted run. Use `npx vitest run`.
- Cypress `.trigger('keydown')` builds a plain `Event` by default, so React never sees `key`. Pass
  `eventConstructor: 'KeyboardEvent'`.
- The Delay and the Noise Gate ship **disabled**; their controls are correctly inert until switched on.
- The exe holds a single-instance lock. A second launch just raises the existing window.
- Inno Setup (`iscc.exe`) is only needed for the installer; `scripts\build-release.ps1` skips that step
  and still produces the standalone exe without it.

## Docs

`docs/prd.md` holds the product requirements and naming conventions. The `SPRINT_*` / `RELEASE_*` /
`TEST_*` markdown files at the repo root are historical sprint reports (partly in Indonesian) that
describe states the code has moved well past — treat them as changelog, not spec.
