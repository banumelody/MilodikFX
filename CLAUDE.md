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

**ASIO** is where the latency actually is. With the Steinberg ASIO SDK present, CMake enables it and
the engine prefers it over everything else:

```powershell
$env:ASIOSDK_DIR = "D:\SDKs\ASIOSDK"      # or pass -AsioSdkPath to build-release.ps1
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DMILODIKFX_ASIO_SDK_PATH="D:/SDKs/ASIOSDK"
```

Measured on the developer's Scarlett 4i4: WASAPI exclusive bottoms out at 144 samples and reports
12 ms; the Focusrite USB ASIO driver goes down to 16 samples and reports **5.5 ms at 32 samples**,
at 0.3 % CPU. Do not compare those two numbers naively -- ASIO reports the driver's real figure
including converters, WASAPI reports an estimate.

Note the SDK is not in the repository: Steinberg's licence permits use but not redistribution.

Without the SDK the engine still works, falling back to WASAPI exclusive mode.

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
safety limiter plus a final clamp — **no stage may be added after it**.

`DSPChainManager::findProcessor<T>()` is a `dynamic_cast` scan returning the *first* instance of a
type, so the chain may contain at most one processor of each type.

#### Post-chain processors

`DSPChainManager::addPostProcessor` registers something that runs *after* the chain **and after the
bypass crossfade** — for signal mixed into the output rather than applied to the guitar. Only
`MetronomeProcessor` uses it. A click routed through the chain would be distorted and cabinet-filtered,
and would vanish the moment global bypass was pressed, which is exactly when you still want the beat.
Anything here carries its own clamp, because it adds level to an already-limited signal.

`findProcessor<T>()` scans both lists, so the one-per-type rule still holds across them.
`getNumProcessors()` counts chain stages only.

#### Tuner

`milodikfx::dsp::TunerAnalyzer` (`src/dsp/TunerAnalyzer.*`) is not in the chain at all. `MainComponent`
taps the input buffer *before* `audioEngine.processBlock`: pitch detection has to see the raw pickup,
since a signal that has been through the overdrive has harmonics that mislead it.

The audio thread only copies into a ring buffer. YIN over a 2048-sample window is around a million
operations — running it inline would overrun a 32-sample callback's 0.67 ms budget several times over —
so a worker thread analyses roughly ten times a second and publishes the result as plain atomics.
Analysis stays off until `POST /api/tuner/enable`; the UI panel is what switches it on.

#### Tempo

One BPM for the whole app, stored by `MetronomeProcessor` and pushed into `DelayProcessor` by the
`global.bpm` setter in `ChainFactory.cpp`. Two independently-edited tempi would let a synced delay
repeat drift against the click. `delay.syncMode` is an enum index into `DelayProcessor::SyncDivision`;
when it is anything but Off, `getEffectiveTimeMs()` derives the time from the tempo and the UI disables
the Time knob rather than leaving it showing a number the delay is not using.

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

`registerEventStream` adds a Server-Sent Events path — a different connection model, since a stream
owns its thread for as long as the page is open. `/api/levels/stream` carries the meters. Two things
that bit here and would again:

- **`ApiJson` pretty-prints.** An SSE event ends at the first line that is not a recognised field, so
  a single `data:` prefix delivered a payload of exactly `{` — an event that arrives, parses to
  nothing, and leaves the stream looking healthy. Every line gets its own prefix.
- **Streams are capped** (`kMaxEventStreams`), because a page in a reload loop would otherwise spawn
  threads without limit. Over the cap the server answers 503.

`subscribeLevels` in `services/api.ts` falls back to polling if the stream never delivers anything.
Measured delivery is ~22 Hz against a 33 ms target: Windows rounds a sleep up to the system timer
granularity.

Endpoints: `/api/effects`, `/api/parameters`, `/api/devices`, `/api/levels`, `/api/tuner`, `/api/ir`,
`/api/midi`, `/api/scenes`, `/api/presets`, `/api/health`.

### Presets and scenes

A preset file is `{schemaVersion, name, savedAt, description, tags, favourite, notes, scenes, state}`.
`state` is exactly what `ParameterRegistry::captureState` produced and nothing else — the metadata sits
*beside* it so the DSP snapshot never depends on how presets happen to be catalogued. Schema 2 files
still load; the new fields simply come back empty.

`savePreset` reads the existing file first and carries its metadata forward. Overwriting a preset to
change how it sounds must not throw away how it was filed.

`milodikfx::preset::SceneManager` holds four slots and stores **only the enable flags**, never
parameter values. That is the load-bearing decision: a scene change mid-song has to be instant and
predictable, and jumping a parameter to a value you cannot see on a control you were not touching is
exactly what it must not do. Full snapshots are what presets are for. Scenes live inside the preset,
with a copy in the settings file so the chain returns as left even when no preset was loaded.
`active` is -1 once the chain has been changed by hand, so nothing claims a slot describes what you
are hearing when it does not.

### MIDI

`milodikfx::midi::MidiController` (`src/midi/MidiController.*`) owns every interaction with
`juce::MidiInput` and marshals device open/close to the message thread — the same discipline
`AudioDeviceController` follows, and for the same reason.

Incoming messages arrive on JUCE's MIDI thread. Writing a parameter from there is safe (processors
hold them as atomics); anything touching files — a program change selecting a preset — is posted to
the message thread.

Two mapping modes, and the difference matters: a footswitch sends 127 on press and 0 on release, so a
`continuous` mapping would need the switch held down to keep the effect on. `toggle` acts on the press
and ignores the release. Mappings live in the settings file under `midi.cc.<n>.{effect,parameter,mode}`
and are saved through `MidiController::onConfigurationChanged` — without that hook a binding only
reached disk if something else happened to have marked the settings dirty.

`handleIncomingMidiMessage` is public deliberately: it is the only way to exercise dispatch without a
physical controller, and `tests/MidiTests.cpp` drives it directly. What is *not* covered is whether a
given footswitch really sends 127/0 — that needs hardware.

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
- The exe holds a single-instance lock, so a second launch silently raises the first window instead of
  starting. When scripting a restart, match processes by executable **path**: a copy named
  `MilodikFX-0.9.0.exe` reports a different process name, and `Stop-Process -Name MilodikFX` walks
  straight past it -- which once left a whole test session talking to a stale build.
- ASIO exposes every installed driver as a device, including ones with no hardware behind them
  (Focusrite's package registers a Thunderbolt driver on machines that have none). The device search
  therefore tries several devices per type rather than giving up on the type after the first failure.
- Inno Setup (`iscc.exe`) is only needed for the installer; `scripts\build-release.ps1` skips that step
  and still produces the standalone exe without it.

## Docs

`docs/prd.md` holds the product requirements and naming conventions. The `SPRINT_*` / `RELEASE_*` /
`TEST_*` markdown files at the repo root are historical sprint reports (partly in Indonesian) that
describe states the code has moved well past — treat them as changelog, not spec.
