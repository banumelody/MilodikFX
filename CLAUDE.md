# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

MilodikFX is a realtime guitar/bass multi-effect processor for Windows, built for one person's own rig.

It ships as a **single self-contained C++/JUCE executable**. The exe runs the audio engine, serves a
loopback-only HTTP API, and hosts the React UI *inside its own window* via Edge WebView2. There is no
browser tab and no separate UI process. The UI bundle is embedded in the exe, so the binary runs on its
own with nothing beside it.

A second target builds the same DSP chain as a **VST3 plugin** plus a JUCE Standalone wrapper, running
the same React UI inside the plugin window. See "Plugin" below.

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

CI (`.github/workflows/ci.yml`) runs two jobs. **frontend** on ubuntu-latest (type-check, lint, vitest,
build). **native** on windows-latest: builds the frontend bundle, configures, builds engine + plugin +
tests, runs the whole native suite, then `pluginval --strictness-level 10` against the VST3.

Two things that bit when that job was added, both of them PowerShell rather than C++:

- **Do not pin the generator.** `-G "Visual Studio 17 2022"` failed with "could not find any instance
  of Visual Studio" — the runner image has moved past it. Letting CMake pick survives image bumps.
- **`Start-Process -Wait`, not a bare call.** pluginval is a GUI-subsystem executable and PowerShell
  does not wait for those: calling it directly returned instantly, leaving `$LASTEXITCODE` holding
  whatever the previous command set, and the step "failed" 150 ms in while pluginval was still
  printing its first test.

The E2E suite is not in CI; it needs a running engine and is run locally. Two things about it:

- **Two concurrent runs against one engine contaminate each other's state.** A run reporting 1-3
  failures is worth repeating alone before believing it.
- **It defaults to the Release build, and says how old that build is.** It used to default to Debug,
  and when no Debug build had been made in over a week it happily ran against the stale one and
  reported a clean pass for code that was not in it. The script now prints the executable's build
  time and warns when anything in `src/` or `frontend/dist` is newer than it — a green suite against
  the wrong binary is worse than a red one, because it reads as evidence.

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
plugin. This is the order it is *built* in, and the default a preset falls back to — since v0.26 the
ten middle stages can be reordered (see "Chain order" below):

```
InputTrim -> NoiseGate -> CleanBoost -> Compressor -> Overdrive -> EQ -> Contour -> NAM -> Cabinet -> Delay -> Reverb -> MasterOut
```

`InputTrim` is ahead of the gate on purpose: with it in front, the gate threshold stays correct
relative to the signal, so swapping guitars means re-dialling one knob rather than two. That ordering
is also why `CleanBoost` cannot double as a trim — it can only add gain, and it sits behind the gate.
The trim registers as `input.gainDb`, on the same card as the app-only `input.mode`.

Each processor derives from `AudioProcessorBase` (`prepareToPlay`/`processBlock`/`reset`) and holds its
parameters as `std::atomic`. `MasterOut` is the only stage that can attenuate, and it carries the
safety limiter plus a final clamp — **no stage may be added after it**.

#### Chain order

The order the chain runs in is a **value**, not a rearrangement of the processors: the objects are
built once and never move. A permutation of up to sixteen stages packs into four bits each, so the
whole order lives in one `std::atomic<uint64_t>` on `DSPChainManager`. The control thread stores a
new packing; the audio thread loads it **once** at the top of a block and uses it for that whole
block. A single 64-bit load either sees the old order or the new one, never half of each, and
nothing allocates on either side.

Two stages are pinned, and neither is a matter of taste:

- **`input` stays first.** The input meter reports what the chain receives as `inputDb + trimDb`
  rather than measuring a second time — arithmetic that is only true while the trim is first.
- **`master` stays last.** It carries the safety limiter and the final clamp.

`DSPChainManager::setOrder` enforces both, so the rule lives in the engine rather than being trusted
to callers. A reorder is audible as a small discontinuity and that is accepted rather than hidden:
the processors are stateful, so running the old and new orders together to crossfade would advance
delay lines and filter memory twice. This is an editing gesture, not a performance one.

Everything above the engine speaks **effect ids**, never indices — `milodikfx::dsp::ChainOrder`
(`src/dsp/ChainOrder.*`) translates. An id survives a version change; an index does not. `applyIds`
is forgiving in both directions because a preset outlives the build that wrote it: unknown ids are
ignored, and stages the list never mentions are put back at their build position rather than dropped.

`/api/chain/order` (GET/PUT). The order persists in the preset (schema 6), in the settings file, and
in the plugin's state blob. **`EffectsHandler` emits the effects in chain order**, which is what makes
a reorder appear in the rack and the chain strip at once — neither component knows the order exists,
they just draw the array they are given.

On the UI side, `useChainReorder` (`frontend/src/hooks/useChainReorder.ts`) drives both surfaces.
**Pointer events, not the HTML5 drag-and-drop API** — that API is awkward in WebView2, cannot be
driven in jsdom, and gives no control over the drag image; pointer capture is the discipline `Knob`
already uses here. Nothing is reordered locally while the pointer moves: the engine is asked once on
release and the rack redraws from what it answers, so a refusal leaves everything where it was with
no local state to unwind. Card rects are measured once at drag start (nothing reflows mid-drag) and
hit-tested via `data-chain-stage`. A keyboard path runs alongside rather than behind it — Enter lifts,
arrows move, Enter drops, Escape cancels — and the ↑▼ buttons stay as the plain fallback.

#### Board placement

Every stage is either **on the board** or not, and off the board means the run loop skips it
entirely -- bit-identical to a chain built without it, asserted in `tests/BoardTests.cpp`.

That is deliberately **not** the same as bypassing. A bypassed delay still runs, which is what keeps
its spillover tail decaying after the footswitch; a delay that is off the board has no tail to keep,
because it is not there. Two gestures, and neither may stand in for the other.

One bit per processor index in `std::atomic<uint32> placedMask`, published exactly like the order and
the bus assignment. **Everything starts placed**, and that default is the migration rule: a preset
or settings file written before v0.30 says nothing about a board, and *absent must mean full* --
reading it as an empty board would silently blank out every rig anyone had saved. `PresetsHandler`
calls `placeAll()` when the key is missing rather than leaving the previous preset's board in place.

`input` and `master` can never come off; `DSPChainManager::setStagePlaced` refuses, the same way
`setOrder` refuses to move them, so the rule lives in the engine rather than in whoever calls it.

`/api/chain/board` (PUT) takes the **complete** set, not a delta. `/api/effects` carries `placed` and
`removable` per stage -- but only for stages the chain actually contains, so the global card and the
metronome are not offered a remove button they have no meaning for.

Placement and order are separate values: taking a block off and putting it back does not move
anything else, which is what stops a rig rearranging itself while it is being edited.

On the UI side `BoardPalette` lists what is off the board, grouped by what each block does to the
signal. It shares `useChainReorder`'s pointer discipline through `paletteProps` rather than growing a
second drag mechanism -- a palette that dragged differently from the rack would feel like a different
app. **The Mixer is never offered on its own**: it arrives with the Splitter and leaves with it,
which is Apple's rule (*"a Mixer automatically appears at the far right"*) and stops anyone building
a board with a mix point and nothing to mix.

The palette scrolls inside its own bounded height. Letting it grow pushed the device panel out of the
sidebar's scroll area, and how far depended on how many blocks happened to be unplaced -- so panels
below it moved for reasons nobody chose. That showed up as an E2E failure in two runs out of four,
which read like a flake and was not one.

Two things the app is never literally short of: the rack always holds Input and Master, so "empty
board" means *no removable stage is placed* rather than a zero count -- a length check would never
fire. The chain strip shows Master for the same reason.

#### Parallel paths (split A/B)

Two extra stages: `SplitProcessor` and `MixerProcessor` (`src/dsp/SplitProcessor.*`,
`MixerProcessor.*`). Between them the chain runs two buffers and each stage in between is assigned to
one of them.

The model is **Logic Pro's Pedalboard, not Fractal's grid**, and the difference is the whole reason
this was affordable: **a stage is assigned to a bus, never duplicated onto one**. There is still
exactly one overdrive — it simply lives on path A or path B. So the registry keeps its flat, stable
id set (`overdrive.drivePct` means one thing), presets do not break, and host automation is
untouched. Duplicating blocks the way Fractal does (Amp 1/Amp 2, Drive 1–3) would mean per-instance
ids through every layer of persistence; that is a different project, and it stays out of scope.

Both brackets are **ordinary stages in the order array**, so dragging them is how the parallel
section is positioned — the same idea as Pedalboard's Router. Neither processes audio on its own:
`DSPChainManager` recognises them **by pointer** (so a reorder can never leave a stale position
behind) and calls `divide()` / `combine()`.

- **Off by default, and off means bit-identical.** With the split disabled the run loop never opens
  path B and the chain is byte-for-byte what it was before any of this existed. `tests/SplitTests.cpp`
  asserts that against a chain built without the stages at all.
- **Bus assignment is a bitmask**, one bit per processor index, published exactly like the order:
  one acquire load per block, no allocation, no lock.
- **Split modes**: `even` (the same signal both ways), `crossover`, and `leftRight`.
- **`crossover`** — Linkwitz-Riley 4th order, two cascaded Butterworth sections per side, so the
  halves sum back to flat. A single section per side would leave a 3 dB bump right at the crossover,
  which on a bass rig is exactly where it is heard. Measured in the suite: 80 Hz into path B at
  0.0002 against 0.2998 into A, and the sum flat within a tenth of a dB from 100 Hz to 3 kHz.
- **`leftRight`** is not a split at all, strictly: with two sources there is nothing to divide, only
  to route. The left input channel becomes path A and the right becomes path B, each duplicated
  across its own path's two channels so the dual-mono stages downstream see a centred signal and the
  mixer's pan is what places it. This is what a guitar with a magnetic and a piezo pickup needs, and
  it is the one thing **stereo input alone cannot do** — one overdrive processes both channels with
  one set of parameter values, so L and R cannot be dialled differently without a bus each. It is
  Fractal's pair of Input blocks ("Left Only" on one, "Right Only" on the other) expressed as a
  split mode, because the engine has a single entry point rather than four.
- **Pan is constant power** (sin/cos), with a `sqrt(2)` compensation so "split with everything
  centred" matches "no split at all". Without it a centred split arrives 3 dB down — the kind of
  thing nobody notices until they A/B it. `mixer.panA/B` is a legitimate modifier target, so a
  pan law that dipped in the middle would make an auto-panner pump.
- **`mixer.invertB`** flips path B's polarity, folded into its gain rather than costing a branch per
  sample. For two pickups on one guitar, not for stereo width: a piezo and a magnetic pickup sense
  the same string from different places and can partially cancel when blended. Not a timing problem
  — both arrive through the same converter on the same sample — so a delay would not fix it.
- **A mixer dragged in front of its split** would strand path B. The run loop folds it back at the
  end of the chain rather than silently discarding half the signal.

A test that hard-pans the two paths apart **cannot tell `leftRight` from `even`** — A's left and B's
right carry the same samples either way. `tests/SplitTests.cpp` therefore leaves both paths centred
and asserts the two modes land on different numbers; the sabotage that motivated it is the same
class as the NAM stereo bug below.

`/api/chain/buses` (PUT) alongside `/api/chain/order`; the assignment persists in the preset
(schema 7) and the settings file. The UI only offers the A/B selector on stages that actually sit
between the split and the mixer — elsewhere it would do nothing.

#### What each stage does with two channels

The chain carries stereo end to end. Every stage falls into one of four kinds, and which one is a
design decision rather than an accident:

| Kind | Stages | Why |
|---|---|---|
| **Dual-mono** — independent state per channel | Overdrive, EQ, Contour, Cabinet (analytic + convolution), InputTrim, CleanBoost, MasterOut | The obvious default: two channels, two sets of filter state |
| **Linked detection** — one detector, same gain both sides | NoiseGate, Compressor | Deliberate. A detector running per channel opens on one side before the other and the stereo image lurches |
| **Mono** — sums to one signal | NAM | A real amp head has one input jack. It **sums** L+R; for a mono rig `0.5*(x+x) == x` is exact, so nothing changed for existing presets |
| **Width generators** | Delay ping-pong, Reverb width, Cabinet `irMode: stereo` | Produce width where the input had none |

`tests/DspTests.cpp` has a **Stereo integrity** suite that pushes a decorrelated signal through every
stage and fails if one collapses the two channels. It exists because every *other* test in the project
feeds mono, so a stage that quietly went mono would pass all of them.

The trap this closes: NAM used to model channel 0 and copy the result over channel 1, on the
documented grounds that the pre-cabinet chain fed both channels the same thing. True of a mono guitar,
and stale the day stereo input existed — the right side of a stereo track was being discarded in
silence. Note that the obvious regression test (*"right-only input should produce output"*) does **not**
catch it: a WaveNet fed silence still emits a DC bias above any sensible floor. What catches it is
symmetry — the same signal down the left and down the right must produce identical output.

`DSPChainManager::findProcessor<T>()` is a `dynamic_cast` scan returning the *first* instance of a
type, so the chain may contain at most one processor of each type.

#### Post-chain processors

`DSPChainManager::addPostProcessor` registers something that runs *after* the chain **and after the
bypass crossfade** — for signal mixed into the output rather than applied to the guitar.
`MetronomeProcessor` and `LooperProcessor` use it. A click routed through the chain would be distorted
and cabinet-filtered, and would vanish the moment global bypass was pressed, which is exactly when you
still want the beat; a loop is the same — you want it playing through a bypass. Anything here carries
its own clamp, because it adds level to an already-limited signal.

`findProcessor<T>()` scans both lists, so the one-per-type rule still holds across them.
`getNumProcessors()` counts chain stages only.

#### Tuner

`milodikfx::dsp::TunerAnalyzer` (`src/dsp/TunerAnalyzer.*`) is not in the chain at all. `MainComponent`
taps the input buffer *before* `audioEngine.processBlock`: pitch detection has to see the raw pickup,
since a signal that has been through the overdrive has harmonics that mislead it.

The audio thread only copies into a ring buffer. YIN is around a million operations — running it inline
would overrun a 32-sample callback's 0.67 ms budget several times over — so a worker thread analyses
roughly ten times a second and publishes the result as plain atomics. Analysis stays off until
`POST /api/tuner/enable`; the UI panel is what switches it on.

The worker **decimates to a ~16 kHz analysis rate** (anti-alias biquad, then take every Nth sample)
before running YIN. This reaches a five-string bass low B (B0, 30.9 Hz) and a four-string low E
(E1, 41.2 Hz) — below the old 55 Hz floor and impossible for a 2048-sample host-rate window to resolve.
It is also *cheaper*: finding a 31 Hz period at 96 kHz directly would need a ~6000-sample window and a
YIN pass roughly ten times heavier, and it makes the cost the same at 44.1 or 192 kHz. The frequency
floor is 27.5 Hz. The pure algorithm holds ±2 cents; through the full decimation path a couple of cents
of bias is normal and well inside the ±5 the display calls in tune.

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

### Modifiers

`milodikfx::dsp::ModulationEngine` (`src/dsp/ModulationEngine.*`) sweeps a parameter with an LFO, the
envelope of the input, or an expression pedal — a tremolo is the master level swept by an LFO, an
auto-wah is the contour frequency swept by picking, a wah is the same swept by a pedal. It runs on the
audio thread from `MainComponent`'s callback, just before the chain reads the parameters (the same spot
the tuner taps the input), and is handed the block's input magnitude for the envelope follower.

It is *not* in the chain and touches no processor directly: it writes the swept value through the
parameter's own `set` closure — the plain atomic store a MIDI CC does, not the registry setter with its
notification. Four fixed slots, no allocation, no lock. The target is a `std::atomic<const
ParameterDescriptor*>` published with release so the audio thread never reads a half-written descriptor;
phase and envelope belong to the audio thread alone, reset through a `needsReset` flag the control
thread raises but never clears.

**Base + offset.** The modulated knob is not inert: it sets the *centre* the sweep rides on.
`effective = clamp(low + source*(high-low) + baseOffset)`, where `baseOffset` shifts the whole window
and defaults to zero — so a modifier added with the panel's low/high sweeps exactly that range until the
knob is turned. A knob turn on a modulated parameter is routed to `ModulationEngine::setBase` through
the registry's `modulatedWriteHook` (the processor atomic is left to the audio thread); the effects
listing reports the centre via `baseValueProvider` so the knob does not chase the sweep; and removing a
modifier returns the parameter to that centre. **Tempo sync**: an LFO with a `syncDivision` derives its
rate from the BPM `MainComponent` pushes each block from the metronome. **Expression**: the `expression`
source reads a live CC through `expressionProvider`, a `std::function` set once at wiring time (never
reassigned), so the audio thread only ever calls it — the same discipline as the `set` closures.
`/api/modifiers` (GET/PUT/DELETE); modifiers persist under `modifier.<slot>.*` (source, low, high, rate,
`expressionCc`, `syncDivision`, `baseOffset`).

### Looper

`milodikfx::dsp::LooperProcessor` (`src/dsp/LooperProcessor.*`) is a single-loop phrase looper, a
post-processor like the metronome so the loop keeps playing across a global bypass and is not run
through the amp and cabinet. One footswitch drives it: `record` is context-sensitive (start → close →
overdub/play), with explicit `stop`, `play`, `clear`. The control thread only ever *requests* an action
through an atomic that the audio thread applies at the top of a block; the record buffer is allocated
once at prepare (60 s max), and `clear` just resets the length rather than zeroing megabytes on the
audio thread. Its own clamp, since it runs after the limiter. `/api/looper` (GET + POST record/stop/
play/clear + PUT level); a footswitch reaches it through `MappingKind::looper` (index = action), and the
recorded loop is deliberately *not* persisted — only the playback level is.

### Drive voicings

`src/dsp/DriveVoicing.h` holds the eight pedal voicings as a table, not a branch per pedal. Type 0 is
`custom` and takes the original untouched code path, so presets written before voicings existed sound
exactly as they did.

The field that carries most of the character is `splitHz`: the signal below it bypasses the clipper and
is added back clean. A Tube Screamer's mid-hump is not an EQ curve, it is the bass being routed around
the clipping stage. Two real filters do the split — a low-pass for the clean path and a high-pass for
the clipped one. Subtracting the low-passed copy *looks* equivalent and is not: at a corner well above
the note the copy is nearly the whole signal but phase-shifted, so the remainder is a sizeable
phase-difference term that then gets the clipper's full gain, and a Tube Screamer measured as
distorting bass harder than a full-range drive.

Cascaded voicings split the gain between stages (`sqrt`), because two full-gain stages drive everything
into a square wave and the DC blocker then centres it — which removes the even harmonics that are the
whole reason to pick an asymmetric voicing.

The engine registers the union of every voicing's parameters once; `DRIVE_CONTROLS` in
`EffectRack.tsx` decides which are shown and what the original pedal called them.

### Spillover

Switching the delay or reverb off stops *feeding* it but keeps the tail decaying, so a scene change
mid-song does not chop the repeats dead. Only the fade **out** is ramped: switching on is instant, so
an enabled effect is bit-identical to the pre-spillover code and the first note after a footswitch is
delayed in full rather than fading in. Once the tail drops below -80 dB the block returns immediately,
so a silent effect costs one atomic read. `spillover` is a per-effect toggle, default on.

### NAM (amp head)

`milodikfx::dsp::NamProcessor` (`src/dsp/NamProcessor.*`) runs a Neural Amp Modeler `.nam` capture — the
amp head — between the tone shaping and the cabinet, where a real head sits between the pedals and the
speaker. The cabinet IR loader models the speaker; NAM models what an IR fundamentally cannot, the way
an amp's distortion changes with how hard you play.

Built via FetchContent from a pinned NeuralAmpModelerCore commit into an **OBJECT** library
(`milodikfx_nam`), not STATIC: each architecture self-registers its config parser through a static
initialiser, and a static archive would let the linker discard those object files as unreferenced, so
`get_dsp()` fails with "No config parser registered". `/arch:AVX2` and `NAM_SAMPLE_FLOAT` are scoped to
that lib; the AVX2 code only runs once a model is loaded, and `NamProcessor::isAvailable()` refuses to
load on a CPU without AVX2, so the portable build still runs on old hardware. Set
`-DMILODIKFX_ENABLE_NAM=OFF` to build without it; `NamProcessor` becomes a passthrough stub.

Realtime discipline, and where the reference plugin's approach is deliberately *not* copied:

- A model is loaded (file read, JSON parse, weight allocation, prewarm) on the calling REST thread,
  never the audio thread — the same rule as the IR loader. The loader builds a whole **Slot** (the model
  plus a resampler already prepared for that model's rate) and publishes it with an atomic exchange.
- The audio thread only ever does a pointer swap: it adopts a staged slot and moves the old one to a
  single `retired` slot. Nothing is allocated or freed in the callback. The reference NAM plugin frees
  the retired model *inside* its audio callback; MilodikFX reaps it from `MainComponent`'s timer instead.
- `process()` is allocation-free once loaded (verified upstream). The model is `Reset()` with a max block
  size the resampler will never exceed, so it never grows its buffers; in Release its `assert` is gone,
  so exceeding it would be an out-of-bounds write, not a crash.

Models run at their trained rate (48 kHz almost always), so `NamResampler` resamples the 96 kHz host
block down and back. It uses a `juce::CatmullRomInterpolator`, not `WindowedSinc`: the latter's 201-tap
kernel, run twice a block, cost more than the model itself (46 % of budget). CatmullRom is 4 taps,
transparent enough for guitar, and drops the whole chain plus a Standard model to ~29 %. The resampler's
reported latency is *measured* by pushing an impulse through at prepare time, not estimated, so the
figure cannot drift from reality. `juce::dsp::ResamplingContainer` was rejected because it has `std::cerr`
and `throw` reachable from its per-block path.

Endpoints: `/api/nam` (library + availability). Models live in `Documents\MilodikFX\NamModels`; the
`nam.namFile` text parameter chooses one, `nam.inputDb`/`nam.outputDb` gain-stage it.

### Audio device

`milodikfx::audio::AudioDeviceController` (`src/audio/AudioDeviceController.*`) owns **every**
interaction with `juce::AudioDeviceManager` and marshals each one onto the message thread. REST handlers
run on arbitrary Winsock threads and must never touch the device manager directly.

Device selection walks preferred types: ASIO -> WASAPI Exclusive -> WASAPI Low Latency -> WASAPI shared
-> DirectSound. Three things it gets right that are easy to break again:

- `initialise(kMaxInputChannels, 2, ...)` is what tells the manager how many input channels are
  *needed*. Calling only `setAudioDeviceSetup` leaves that at zero and opens the device output-only.
  It is a maximum, not a demand — a two-in interface still opens with two. It used to be literally
  `2`, which made every jack past the second unreachable: on a Scarlett 4i4 the two rear line inputs
  could not be selected at all, because the device was never opened wide enough to see them. The
  same 4i4 now reports six (Input 1–4 plus the two loopback channels).
- The saved state is only restored when it is a real `DEVICESETUP` element. The manager silently ignores
  an unrecognised one, opens the default device, and reports no error — which looks exactly like success.
- Buffer/rate **preferences** change only when the user asks. Adopting whatever the device happened to
  open at once made the app treat a 2048-sample fallback as its target forever after.

On the developer's Scarlett this reaches 288 samples at 96 kHz, 12 ms round trip, ~2.4 % CPU in Release.

#### Input port mapping

Which physical jack feeds the engine's left channel and which feeds its right. Stored by channel
**name** in the settings file (`audio.inputPortL/R`), never in a preset: it describes the cables in
the rig rather than a sound, so a preset carrying it would be wrong the moment it was opened on
another interface — and wrong silently. A name also survives a device reopening with a different
channel count, which an index does not.

The load-bearing subtlety: **JUCE hands the callback one pointer per *active* channel, in order —
not one per physical port.** With only ports 2 and 4 streaming, `inputChannelData[0]` is port 2. So
a port's position is its rank among the active channels, it has to be recomputed **every time the
device changes** (`resolveInputPorts` from `audioDeviceAboutToStart`), and resolving it once at
startup would survive exactly until the first device change. The failure is silent: the wrong jack,
or nothing at all. `AudioDeviceController::inputPositionFor` is a pure static so it can be tested
without hardware — `tests/InputRoutingTests.cpp` covers the sparse-mask case directly. A name the
device does not have falls back to the first/second channel rather than to silence, and logs.

`GET /api/devices` carries `inputRouting {ports, left, right}`; `POST` accepts `inputPortLeft` /
`inputPortRight`, applied before any device change in the same body so the routing lands on the
device about to be opened. Hidden in the plugin — the host owns the I/O.

### Performance

`tests/PerformanceTests.cpp` measures the whole chain against a 32-sample block at 96 kHz — a 0.33 ms
budget, which is what every realtime rule here exists to protect. The absolute figures it logs are from
a Debug build and are not the shipping numbers; the assertions that bite are the build-independent
ratios (a decayed spillover tail must cost less than a ringing one, no voicing may cost 3x another,
cost must not creep between runs).

Measured on the developer's rig in Release, ASIO 96 kHz / 32 samples / 4.33 ms round trip: **7.6 % DSP**
with every effect on, **16.3 %** worst case (Marshall-in-a-Box at 8x oversampling, 90 % drive).

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
`/api/nam`, `/api/midi`, `/api/scenes`, `/api/presets`, `/api/modifiers`, `/api/looper`, `/api/health`,
`/api/update`.

`/api/update` (`UpdateHandler`) reads the newest tag from GitHub Releases and compares it to the
compiled-in `MILODIKFX_VERSION`, returning `{current, latest, updateAvailable, url}`; the UI raises a
dismissible banner on it. The network read runs on the Winsock worker thread with a short timeout, and a
success is cached for 30 min so a reload loop cannot exhaust GitHub's unauthenticated rate limit. The
version comparison is a pure function (`isNewerVersion`) and is unit-tested; the network fetch is not.
External links in the UI (sponsor page, a release) open via `UiWebView::newWindowAttemptingToLoad`, which
hands a `target=_blank` off to the system browser rather than a chromeless WebView2 popup.

### Plugin (VST3)

`src/plugin/` is the same engine in a host, and the load-bearing insight is that **`RestApiDispatcher`
and `HttpHandler` were never tied to a socket** — they take `(method, path, query, body)` and hand back
`(status, body)`. So the plugin reuses the identical handler set through a different transport.

- **`PluginBackend`** owns that dispatcher plus its own `SceneManager`, `ChannelStore` and
  `PinnedControls`, and registers effects/parameters/presets/scenes/pins/ir/nam/levels/health. It is
  built lazily on first editor open, so a render farm of unopened instances pays nothing for it.
  Deliberately *not* registered: `devices` and `midi` (the host owns both), `looper`/`metronome`
  (not in the plugin's chain), `update` (a plugin has no business calling GitHub), `history` (the
  DAW's own undo stack is the one that matters, and a second one fighting it is worse than none) and
  `chain` -- so reordering and board placement are app-only. `getChainOrder()` simply fails there,
  `chainOrder` stays null, and the palette and drag handles never render; the rack shows every stage
  because an absent `placed` flag reads as placed.
- **`PluginEditor`** hosts a JUCE 8 `WebBrowserComponent` with `withResourceProvider` serving the
  *same* embedded bundle the exe uses, and `withNativeFunction("milodikfxApi", …)` for the calls.
  **A plugin must never open a port** — several instances in one project would fight over it. A
  resource provider is handed only a URL, no method and no body, which is exactly why writes go
  through the native function instead. Falls back to `GenericAudioProcessorEditor` without WebView2.
- **`services/transport.ts`** is the frontend half: `fetch` in the app, JUCE's
  `__juce__invoke`/`__juce__complete` bridge in the plugin, chosen once. The protocol is spelled out
  there rather than importing JUCE's `juce.js`, which is served at runtime and would stop the bundle
  being one self-contained embeddable file. SSE does not exist in the plugin, so `subscribeLevels`
  takes the polling path it already had as a fallback. `isPluginHost()` hides the device, MIDI, update
  and looper panels — the things the host owns.

Four things the plugin gets right that are easy to break again:

- **No looper, no metronome** (`ChainOptions{false,false}`). The looper allocates its entire 60-second
  record buffer at prepare — 23 MB at 48 kHz, 46 MB at 96 kHz, *per instance* — for controls that do
  not exist in a host. With no metronome to hold the tempo, `global.bpm` falls back to the delay as
  its owner, so there is still exactly one BPM.
- **The host owns the tempo.** `followHostTempo()` reads `getPlayHead()` every block; a synced delay
  has to land on the project grid, not on a number typed into the plugin.
- **Latency is reported and kept current.** Overdrive oversampling *plus* `NamProcessor`'s measured
  resampler latency, re-checked on the timer, because a model loads long after `prepareToPlay` and a
  stale figure silently misaligns the track against every other one. At a 48 kHz session the NAM
  resampler is a passthrough and adds nothing. Known limit: a bypassed instance is not
  latency-compensated for the reported figure.
- **Parameters are polled, not listened to.** `pollHostParameters()` compares cached floats against
  the APVTS atomics at the top of `processBlock` and applies what moved, straight through the
  descriptor's `set` closure. The old listener did a linear scan of `juce::String` comparisons on
  whichever thread the host automated from — including the audio thread.

The timer also reaps retired NAM models (`collectGarbage`), which nothing did before: every model
change leaked one until the instance was destroyed.

**Modifiers and the expression pedal.** `ModulationEngine` runs where the app runs it -- on the audio
thread, just before the chain reads the parameters -- fed the block's input magnitude and, for a
tempo-locked LFO, the BPM the *delay* holds here. MIDI input exists for exactly one reason: a wah is
the contour frequency swept by a pedal, and in a host that pedal arrives as controller messages rather
than from a device the plugin opens. `readControllers()` stores their values into atomics and does
**no mapping dispatch** -- recalling a scene must be posted to the message thread, and posting
allocates, which is precisely why the app does that work on JUCE's MIDI thread instead. Footswitch
scenes and channels in the plugin are therefore still absent, deliberately and by name.

Three interactions there that are each a bug if reversed:

- Host automation of a modulated parameter goes to `setBase` first, so the write moves the sweep's
  centre. Writing the atomic directly loses to the sweep every block.
- `syncHostParametersFromChain` reports the *centre*, not the swept sample, or a preset load captures
  wherever the LFO happened to be and pins the chain there.
- Persisted state stores `baseOffset`, so a project saved mid-sweep reopens on the centre.

`tests/smoke.ps1` does not cover the plugin. **`pluginval --strictness-level 10` does**, and CI runs it
on every push — it is what catches state round-trips, odd bus layouts, extreme block sizes and
parameter access from the wrong thread.

### Presets and scenes

A preset file is `{schemaVersion, name, savedAt, description, tags, favourite, notes, scenes, state}`.
`state` is exactly what `ParameterRegistry::captureState` produced and nothing else — the metadata sits
*beside* it so the DSP snapshot never depends on how presets happen to be catalogued. Schema 2 files
still load; the new fields simply come back empty.

`savePreset` reads the existing file first and carries its metadata forward. Overwriting a preset to
change how it sounds must not throw away how it was filed.

`milodikfx::api::UndoHistory` stores the same kind of snapshot. A step is committed by
`MainComponent`'s timer once the chain has been still for `kHistorySettleMs`, never per write:
dragging a knob produces a write per frame, and one step each would mean fifty presses of Ctrl+Z to
get back. After applying, the baseline is re-read from the chain rather than trusted, because a
parameter can clamp what it was given and a baseline that disagreed would make the next commit record
a step nobody took.

`milodikfx::preset::SceneManager` holds four slots and stores the enable flags and, when a channel
store is attached, a channel index per effect — never raw parameter values. That is the load-bearing
decision: a scene change mid-song has to be instant and predictable, and jumping a parameter to a
value you cannot see on a control you were not touching is exactly what it must not do. A channel is
still something you can see — a named tab you built — so recalling one is not a hidden jump. Full
snapshots are what presets are for. Scenes live inside the preset, with a copy in the settings file so
the chain returns as left even when no preset was loaded. `active` is -1 once the chain has been
changed by hand, so nothing claims a slot describes what you are hearing when it does not.

#### Channels

`milodikfx::preset::ChannelStore` (`src/preset/ChannelStore.*`) gives every effect up to four saved
sounds — channels A/B/C/D, the way a Fractal FM9 block does. Switching a channel saves the live values
into the outgoing channel first (so edits are kept), then applies the chosen one through the registry
setters (atomics, smoothed — the same discipline a MIDI CC writes with, and it never calls `reset()`,
so a delay/reverb tail rings on across a switch). Channels persist inside the preset (schema 4) and the
settings file. The API lives on the effects surface: `PUT /api/effects/<id>/channel {value}` selects
one and returns the effect's new parameter values; `/api/effects` carries `channel` and `channels` per
effect. `commitActive()` is called before a preset save so the stored active channel matches the state
snapshot. Extends the scene rule rather than breaking it — see above.

### MIDI

`milodikfx::midi::MidiController` (`src/midi/MidiController.*`) owns every interaction with
`juce::MidiInput` and marshals device open/close to the message thread — the same discipline
`AudioDeviceController` follows, and for the same reason.

Incoming messages arrive on JUCE's MIDI thread. Writing a parameter from there is safe (processors
hold them as atomics); anything touching files — a program change selecting a preset — is posted to
the message thread. A mapping's `kind` picks what it drives: a `parameter` (or an effect's `enabled`
switch), a `scene` (recalled on press), a `channel` of one effect (selected on press), or a `looper`
action (record/stop/clear on press). Scene and channel recalls touch the scene/channel stores, which are
not built for the MIDI thread, so they are posted to the message thread the same way a program change is
— `onSceneRecall`/`onChannelSelect`/`onLooperAction` in `MainComponent` do the actual work there.

Two mapping modes, and the difference matters: a footswitch sends 127 on press and 0 on release, so a
`continuous` mapping would need the switch held down to keep the effect on. `toggle` acts on the press
and ignores the release (scene/channel targets always act on the press). Mappings live in the settings
file under `midi.cc.<n>.{kind,effect,parameter,index,mode}` and are saved through
`MidiController::onConfigurationChanged` — without that hook a binding only reached disk if something
else happened to have marked the settings dirty. A file written before `kind` existed reads back as a
parameter mapping.

`handleIncomingMidiMessage` is public deliberately: it is the only way to exercise dispatch without a
physical controller, and `tests/MidiTests.cpp` drives it directly. What is *not* covered is whether a
given footswitch really sends 127/0 — that needs hardware.

Three more things the MIDI path does, all aimed at footswitches like the M-Vave Chocolate:

- **The UI has to hear about MIDI-driven changes.** A footswitch recalling a scene, or a CC moving a
  parameter, is a change the UI did not make — so nothing would otherwise tell it to redraw, and the
  giant Perform scene buttons would not light up. `LevelsHandler` carries a `chainVersion` counter in
  the meter payload (already streaming ~22 Hz); `MainComponent::bumpChainVersion()` nudges it from the
  MIDI callbacks (CC-parameter, scene, channel, program change) and *only* those — a UI knob drag must
  not bump it, or every client would refetch and fight the drag. The UI watches the number and refetches
  `/api/effects` + `/api/scenes` (debounced) when it moves.
- **Auto-reconnect.** The device is opened once at startup; a wireless pedal that sleeps or drops would
  otherwise stay dead until the panel was reopened. `MainComponent`'s timer rescans every ~4 s and
  reopens the *desired* device (`desiredMidiDevice`, persisted as `midi.device`, not the currently-open
  name) the moment `listDevices()` reports it back.
- **Bluetooth LE.** JUCE's default WinMM backend cannot see BLE MIDI at all. `MILODIKFX_WINRT_MIDI`
  (CMake, default ON) sets `JUCE_USE_WINRT_MIDI=1` so paired BLE devices appear alongside USB ones; it
  is a build option with a WinMM fallback because JUCE marks WinRT MIDI experimental. The
  **learn-4 wizard** in the MIDI panel is pure frontend: it arms scene-learn for slots 1–4 in turn,
  advancing as each press binds, so any 4-button footswitch is set up without hardcoding its (firmware-
  and user-variable) CC numbers. BLE end-to-end is unverified here — it needs the real hardware.

### Frontend

`main.tsx` -> `App.tsx` -> the components under `components/` (EffectRack, Knob, Toggle, ChainStrip,
the meters, DeviceSettings, TunerDisplay, TempoPanel, SceneGrid, PresetControls, MidiMapping, NamPanel,
ModulationPanel, LooperPanel, UpdateBanner, AppFooter, PerformView) plus `services/api.ts` and the
`useLooper` hook.

`App` holds a **Perform | Edit** view toggle, remembered in localStorage. **Edit** is the dense rack —
the default, unchanged. **Perform** (`PerformView`) is the stage-facing screen: big preset name with
prev/next, big BPM + tap tempo, four giant scene buttons, wide In/Out LED meters, big Tuner/Bypass/Mute,
and a large tuner that replaces the scene grid when on. Its keys (1–4 scenes, arrows presets, T tap) are
scoped to when it is mounted; Esc (mute) and B (bypass) stay global in `App`. The heavy children are
`memo`ised and `App`'s callbacks are `useCallback`-stable, so the ~22 Hz meter stream does not re-render
the rack — the components take the same props across a meter frame.

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
  and still produces the standalone exe without it. `winget install JRSoftware.InnoSetup` puts it under
  `%LOCALAPPDATA%\Programs\Inno Setup 6`, not Program Files — the script looks in all three, because
  leaving the per-user path out meant a machine that *had* Inno Setup still silently skipped the step.

## Docs

- `README.md` — user-facing overview, download and build instructions, licence.
- `docs/prd.md` holds the original product requirements and naming conventions. It predates most of the
  current feature set (tuner, MIDI, scenes, NAM, the eight drive voicings) — treat it as the founding
  spec, not a current inventory. `README.md` and this file are the up-to-date picture.
- `docs/roadmap.md` — the living backlog, with each item's status and the design decisions behind it.
- `docs/nam-plan.md` — the NAM (amp head) design and the measured numbers it was decided on.

The old `SPRINT_*` / `RELEASE_*` / `TEST_*` reports and the Electron-era plans were removed; they
described states the code moved well past. Release history lives in the GitHub Releases.
