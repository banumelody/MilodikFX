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

### Frontend — build, test, lint (exact commands)

- Setup
  - cd frontend
  - npm ci

- Dev server (port 3000)
  - npm run dev
  - Note: Vite dev server is configured to listen on port 3000 (frontend/vite.config.ts). Do not run the built MilodikFX.exe at the same time — port 3000 conflicts.

- Build for embedding into exe (production)
  - npm run build
  - Output: frontend/dist (contains index.html and assets)

- Preview built dist
  - npm run preview (serves dist via Vite preview)

- Unit tests
  - Run all frontend tests: npm run test
  - Run a single frontend test: npm run test -- -t "<pattern>" (or npx vitest -t "<pattern>")
  - UI test runner: npm run test:ui
  - Coverage: npm run coverage

- Lint & format
  - npm run lint
  - npm run lint:fix
  - npm run format
  - Type-check: npm run type-check

- E2E (Cypress)
  - Open Cypress UI: npm run e2e
  - Run headless (CI): npm run e2e:headless

### Backend — build & tests (Windows / CMake)

- Configure + build (Debug)
  - cmake -S . -B build -G "Visual Studio 17 2022" -A x64
  - cmake --build build --config Debug --parallel

- Build Release exe
  - cmake --build build --config Release --target MilodikFX --parallel

- Run backend unit tests (JUCE UnitTest binary)
  - Build tests: cmake --build build --config Debug --target MilodikFX_tests
  - Run via CTest (invokes the test binary):
    - ctest -C Debug -R MilodikFX_UnitTests --output-on-failure
  - Or run executable directly:
    - build\MilodikFX_tests_artefacts\Debug\MilodikFX_tests.exe

- Run smoke tests
  - The project registers smoke tests that invoke PowerShell scripts under tests/:
    - ctest -C Debug -R MilodikFX_Smoke
  - Or run the scripts directly (PowerShell):
    - powershell -ExecutionPolicy Bypass -File tests\smoke.ps1 -AppPath "<path-to-MilodikFX.exe>"

- Run a single backend test (notes)
  - The JUCE UnitTest framework in this repository aggregates many UnitTest classes into one binary. There is no built-in per-test ctest entry by default.
  - Quick options:
    - Run the test executable and filter/grep its output for the test you care about.
    - Temporarily modify tests/UnitTests.cpp to register only the target UnitTest while developing a single-case fix.
  - Recommended long-term: add per-UnitTest add_test entries in CMake so ctest -R <testname> can run a specific test.

### Running the app (manual)

- Ensure frontend/dist exists before building the exe. CMake post-build copies frontend/dist into resources/ui/web/.
- Start (Release build example):
  - build\MilodikFX_artefacts\Release\MilodikFX.exe
  - Open http://localhost:3000 in a browser
- Logs:
  - milodikfx.log next to the executable (e.g., build\MilodikFX_artefacts\Debug\milodikfx.log)
- If the browser downloads index.html instead of rendering, check:
  - resources/ui/web/index.html is present
  - src/ui/WebServer.cpp getMimeType() and serveFile() logic

### Key runtime & repo conventions (for Copilot sessions)

- Hybrid architecture: C++ JUCE backend + React frontend
  - Static frontend files served from resources/ui/web/ (CMake copies frontend/dist -> resources/ui/web)
  - REST API root: /api/*
  - Common REST endpoints used by the frontend (quick reference):
    - GET /api/devices
    - GET/PUT /api/parameters/master-volume
    - GET /api/parameters and GET /api/parameters/{effect}/{param}
    - GET/POST/PUT /api/effects/{effect}/*
    - GET /api/levels
    - GET/POST /api/presets (save/load)

- WebServer: src/ui/WebServer.cpp
  - Native Winsock server: non-blocking accept loop and per-connection handling
  - Serves files from the resources/ui/web subdirectory of the exe
  - Dispatches /api requests to RestApiDispatcher and handlers under src/api/

- DSP and audio:
  - src/dsp/ contains processor implementations (Gain, Overdrive, EQ, Compressor, Reverb, ToneStack)
  - DSPChainManager exposes addProcessor() and findProcessor<T>() used by REST handlers to apply parameter changes
  - AudioEngine in src/audio/ wires the chain and processBlock()
  - MainComponent handles startup order: WebServer created first, then DSP processors, then audio device init (async)

- Settings & Presets:
  - Settings file: %APPDATA%\MilodikFX\MilodikFX.settings (created via juce::PropertiesFile in Main.cpp)
  - Presets: PresetManager stores files under user Documents by default (MilodikFX/Presets)

- Tests locations:
  - Frontend tests (Vitest + Cypress): frontend/
  - Backend tests (JUCE UnitTest): tests/UnitTests.cpp (built into MilodikFX_tests)

### CI / GitHub Actions

- Pipeline: .github/workflows/ci.yml builds frontend, runs Vitest, starts a preview server and runs Cypress headless E2E.
- CI uses Node on ubuntu-latest; backend native build is not performed in that workflow.

### AI / Copilot helper files

- This repository includes Copilot/skill helpers under .github/skills/ (e.g., juce-electron-hybrid-audio SKILL.md)
- CI and run scripts referenced above are the best places to pull authoritative commands for Copilot sessions.

### Files to open first when asked to change behavior

- src/ui/WebServer.cpp — static file serving, HTTP parsing and routing
- src/api/*Handler.* — REST handlers (ParametersHandler, EffectsHandler, PresetsHandler, DevicesHandler, LevelsHandler)
- src/dsp/*Processor.* and src/dsp/DSPChainManager.* — DSP implementations and chain lookup
- src/audio/AudioEngine.* and src/MainComponent.* — audio startup and wiring
- frontend/src/services/audioApi.ts and frontend/src/components/* — frontend integration and UI hooks
- CMakeLists.txt — build steps and resource copying
- .github/workflows/ci.yml — CI build/test steps for frontend

### Suggested improvements (quick wins)

- Replace fragile string-based JSON parsing in REST handlers with JUCE JSON APIs (juce::var / juce::JSON::parse) for robustness and correct Content-Type handling.
- Add per-UnitTest CTest entries in CMake (add_test for each UnitTest registration) so `ctest -R <testname>` can run specific backend tests.
- Improve HTTP request logging in `src/ui/WebServer.cpp`: log the request-line, headers length, Content-Length, and response status code to make intermittent failures reproducible.
- Add a lightweight local integration wrapper (scripts/run-local-e2e.ps1 for Windows and scripts/run-local-e2e.sh for POSIX) that:
  1. Builds frontend (cd frontend && npm ci && npm run build)
  2. Builds the exe (cmake --build build --config Debug)
  3. Starts the exe in background and waits for readiness
  4. Performs a curl/Invoke-WebRequest smoke checklist (GET /index.html, GET /api/levels, GET/PUT /api/parameters/master-volume, GET /api/presets)
  5. Stops the exe and returns a consolidated exit code (0 success, non-zero failure)
  (A more fully featured script already exists under `.github/scripts/run-e2e-tests.*` — keep the local wrapper minimal and idempotent.)
- Add a simple health endpoint `/api/health` that returns `{"status":"ok"}` and use it for readiness probes in CI/scripts.
- Explicit port guidance: avoid running Vite dev server on port `3000` while the EXE binds `3000`. Use Vite preview on `5173` for CI (`npx vite preview --port 5173 --strictPort`) or change `frontend/vite.config.ts` when developing both fronts simultaneously. When previewing, set `CYPRESS_BASE_URL` / `VITE_API_BASE` appropriately.
- UI component guidance for the custom rotary knob: capture exact design tokens (diameter, knob face gradient, indicator thickness, shadow offsets, label font-size) in `frontend/src/components/Knob/DESIGN.md` so pixel parity is reproducible across redesigns and tests.
- Consider adding a CTest integration target that runs the minimal integration wrapper so `ctest -C Debug -R integration-local` can run the end-to-end smoke checks as part of local CI verification.

---

> Note: this file augments the existing instructions with explicit commands and reference locations. Keep the top-level Build/Stack sections concise; expand details here when workflow changes.

