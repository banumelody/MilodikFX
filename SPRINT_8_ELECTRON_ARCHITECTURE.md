# Sprint 8: Electron Migration - Architecture & Implementation Plan

## Objective

Migrate MilodikFX from JUCE WebBrowserComponent to Electron + Native C++ Module, creating a more robust, maintainable hybrid application while preserving the modern React frontend and C++ audio engine.

**Status**: Planning Phase
**Target Release**: v0.9.0
**Estimated Duration**: 15 days

---

## Architecture Overview

### Current (v0.8.0 - JUCE WebBrowserComponent)

```
MilodikFX.exe (JUCE Application)
├── WebBrowserComponent (Embedded Browser)
│   └── React UI (localhost:3000)
│       ├── Components (28 reusable)
│       ├── TailwindCSS (dark theme)
│       └── React Context (state)
├── WebServer (HTTP API)
│   ├── /api/parameters
│   ├── /api/devices
│   ├── /stream/meters (SSE)
│   └── /api/presets
└── Audio Engine (JUCE)
    ├── AudioDeviceManager
    ├── DSP Chain (6 processors)
    ├── ASIO/WASAPI
    └── Parameter Queue (lock-free)
```

**Communication**: HTTP REST API (localhost:3000)
**Package Size**: 6.7 MB
**Startup Time**: ~2 seconds

---

### Target (v0.9.0 - Electron + Native Module)

```
MilodikFX.exe (Electron Main Process)
├── Node.js Runtime
│   ├── IPC Server (Electron.ipcMain)
│   └── Native Module Loader
├── Electron Renderer (React UI)
│   ├── React 18 (components, hooks)
│   ├── TailwindCSS (dark theme)
│   ├── Zustand (state management)
│   └── IPC Client (preload, contextBridge)
├── C++ Native Module (napi)
│   ├── JUCE Audio Engine Binding
│   ├── Parameter Control
│   ├── Device Management
│   └── Meter Data Export
└── Native Windows Code
    ├── ASIO SDK (optional)
    └── WASAPI
```

**Communication**: Electron IPC (Main ↔ Renderer)
**Package Size**: ~150-200 MB (includes Chromium)
**Startup Time**: ~5-10 seconds

---

## Key Changes

### 1. Build System
- **Before**: CMake + JUCE + C++
- **After**: npm + node-gyp + webpack/vite + CMake (for native module only)

### 2. Communication Protocol
- **Before**: HTTP REST `/api/parameters` with JSON
- **After**: Electron IPC `ipcRenderer.invoke('setParameter', {...})`

### 3. Dependency Management
- **Before**: vcpkg + JUCE FetchContent
- **After**: npm packages + node-gyp

### 4. Entry Point
- **Before**: JUCE MainComponent
- **After**: Node.js main.js + Electron BrowserWindow

---

## Implementation Phases

### Phase 1: Electron Project Setup (Days 1-3)

#### Day 1: Initialize Electron Project
1. Create new Electron project structure
2. Setup `electron-main` entry point (main.js)
3. Configure `electron-preload` for IPC bridge
4. Setup webpack/vite for React bundling
5. Create package.json with dependencies

**Files to create**:
- `main.js` (Electron main process)
- `preload.js` (IPC bridge setup)
- `electron/background.js` (window management)
- `webpack.config.js` or `vite.config.js`
- New `package.json` structure

**Key packages**:
```json
{
  "dependencies": {
    "electron": "^latest",
    "react": "^18.2.0",
    "react-dom": "^18.2.0",
    "zustand": "^4.3.0"
  },
  "devDependencies": {
    "node-gyp": "^9.3.0",
    "electron-builder": "^latest",
    "webpack": "^5.0.0"
  }
}
```

#### Day 2: IPC Communication Setup
1. Implement `ipcMain` message handlers in main.js
2. Implement `ipcRenderer` calls in React components
3. Create preload.js with `contextBridge` API
4. Setup error handling for IPC failures
5. Create IPC types/interfaces

**IPC Message Types**:
```typescript
// From Renderer (React)
'setParameter': { effect: string; parameter: string; value: number }
'getDevices': void
'setDevice': { deviceId: string }
'subscribeMeterUpdates': void
'savePreset': { name: string; data: PresetData }
'loadPreset': { id: string }

// From Main (Node.js)
'meterUpdate': { input: number; output: number; peaks: number[] }
'parameterChanged': { effect: string; parameter: string; value: number }
'deviceChanged': { deviceId: string }
'error': { code: string; message: string }
```

#### Day 3: Webpack/Build Configuration
1. Configure webpack for React app bundling
2. Setup dev server for development
3. Configure Electron auto-reload (nodemon)
4. Create build scripts (dev, build, release)
5. Test dev environment startup

**npm scripts**:
```json
{
  "start": "node-dev main.js",
  "dev": "webpack --watch & npm start",
  "build": "webpack --mode=production",
  "dist": "electron-builder"
}
```

---

### Phase 2: C++ Native Module (Days 4-8)

#### Day 4: Node.js Binding Setup
1. Initialize native addon project (node-gyp structure)
2. Install build dependencies (Windows SDK, MSVC)
3. Create `binding.gyp` configuration
4. Setup NAPI (Node-API) project structure
5. Test hello-world binding compilation

**Directory structure**:
```
src/native/
├── binding.gyp
├── src/
│   ├── binding.cc (main entry)
│   ├── audio_engine.h
│   ├── audio_engine.cc
│   └── module.cc (NAPI exports)
└── build/ (generated)
```

#### Day 5: JUCE Audio Engine Wrapper
1. Extract audio processing code from existing JUCE MainComponent
2. Create `AudioEngineWrapper` class that doesn't use JUCE UI
3. Expose these methods as NAPI functions:
   - `init()` - initialize audio device
   - `setParameter(effect, param, value)`
   - `getParameter(effect, param)`
   - `getMeterData()` - return current meter readings
   - `getDeviceList()`
   - `setDevice(deviceId)`
   - `start()` / `stop()` audio processing

**C++ side (binding.cc)**:
```cpp
Napi::Value InitAudio(const Napi::CallbackInfo& info) {
  // Call native JUCE code
  gAudioEngine->initialize();
  return Napi::Boolean::New(info.Env(), true);
}

Napi::Value SetParameter(const Napi::CallbackInfo& info) {
  std::string effect = info[0].As<Napi::String>();
  std::string param = info[1].As<Napi::String>();
  float value = info[2].As<Napi::Number>();
  
  gAudioEngine->setParameter(effect, param, value);
  return info.Env().Undefined();
}
```

#### Day 6: Meter Data Streaming
1. Implement meter data export from audio engine
2. Create background thread to sample meter data
3. Expose meter data via NAPI callback
4. Implement periodic polling (10ms interval)

**JavaScript side**:
```typescript
// In preload.js
contextBridge.exposeInMainWorld('audioEngine', {
  setParameter: (effect, param, value) => 
    ipcRenderer.invoke('setParameter', {effect, param, value}),
  onMeterUpdate: (callback) => 
    ipcRenderer.on('meterUpdate', (event, data) => callback(data))
});
```

#### Day 7: Device Management
1. Expose device enumeration from JUCE
2. Implement device selection switching
3. Add device list caching
4. Handle device change events

#### Day 8: Native Module Testing
1. Compile native module (`npm run build-native`)
2. Test JavaScript binding calls
3. Verify audio processing works
4. Load testing (100+ param updates/sec)
5. Memory profiling

---

### Phase 3: Communication Refactoring (Days 9-11)

#### Day 9: Remove HTTP Dependency
1. Remove WebServer.h/cpp from build
2. Remove localhost:3000 startup
3. Remove fetch() calls from components
4. Update CMakeLists.txt to exclude WebServer

#### Day 10: Replace HTTP with IPC
1. Create `messageBridge.ts` using IPC (not HTTP)
2. Update all components to use new API:
   - `Knob` → `ipcRenderer.invoke('setParameter', ...)`
   - `Meter` → `ipcRenderer.on('meterUpdate', ...)`
   - `DeviceSelector` → `ipcRenderer.invoke('setDevice', ...)`
3. Implement debouncing in main.js (50ms batching)
4. Add error handling with toast notifications

**messageBridge.ts** (NEW):
```typescript
export const messageBridge = {
  setParameter: (effect: string, param: string, value: number) => 
    window.audioEngine.setParameter(effect, param, value),
    
  getDevices: () => 
    ipcRenderer.invoke('getDevices'),
    
  onMeterUpdate: (callback: (data: MeterData) => void) => 
    window.audioEngine.onMeterUpdate(callback)
};
```

#### Day 11: Main Process IPC Handlers
1. Implement all `ipcMain.handle()` handlers
2. Add parameter debouncing logic
3. Add error handling and retry logic
4. Add logging for debugging

**main.js**:
```javascript
ipcMain.handle('setParameter', async (event, {effect, param, value}) => {
  // Debounce: batch changes within 50ms window
  parameterQueue.push({effect, param, value});
  clearTimeout(debounceTimer);
  
  debounceTimer = setTimeout(() => {
    nativeModule.setParameters(parameterQueue);
    parameterQueue = [];
  }, 50);
});

ipcMain.handle('getDevices', async (event) => {
  return nativeModule.getDeviceList();
});
```

---

### Phase 4: Integration & Polish (Days 12-13)

#### Day 12: Full Integration Testing
1. Test React component → IPC → Native module → Audio → Meter feedback
2. Test parameter changes (all 6 DSP processors)
3. Test device switching
4. Test preset save/load
5. Verify no memory leaks
6. Check startup performance

#### Day 13: Bug Fixes & Optimization
1. Fix any IPC communication issues
2. Optimize meter streaming (10ms vs 50ms interval)
3. Reduce startup time if possible
4. Refine error messages
5. Update documentation

---

### Phase 5: Release & Testing (Days 14-15)

#### Day 14: E2E Testing
1. Automated tests for full audio flow
2. Load testing (500+ param changes/sec)
3. Performance benchmarking
4. Cross-browser/Electron version testing
5. Window state persistence testing

#### Day 15: Release & Documentation
1. Build release executable (`npm run dist`)
2. Create release notes for v0.9.0
3. Update README with Electron architecture
4. Publish GitHub Release with executable
5. Update documentation files

---

## File Changes Summary

### Files to Create
- `main.js` (Electron entry point)
- `preload.js` (IPC bridge)
- `src/electron/` (Electron-specific code)
- `src/native/` (Node.js native module)
- `webpack.config.js`
- New `package.json`

### Files to Remove
- `src/ui/WebServer.h`
- `src/ui/WebServer.cpp`
- `src/ui/WebSocketServer.h` (if exists)

### Files to Modify
- `src/MainComponent.cpp` (remove WebServer init)
- `src/MainComponent.h` (remove WebServer)
- `frontend/src/services/messageBridge.ts` (IPC instead of HTTP)
- `frontend/src/components/Knob.tsx` (use IPC)
- `frontend/src/components/Meter.tsx` (use IPC)
- All components using parameter updates
- `CMakeLists.txt` (remove WebServer, add native module)

### New Package Dependencies
```json
{
  "electron": "^27.0.0",
  "zustand": "^4.3.0",
  "electron-builder": "^24.0.0",
  "node-gyp": "^9.3.0"
}
```

---

## Communication Protocol

### Renderer → Main (IPC Invoke)

**setParameter**
```typescript
ipcRenderer.invoke('setParameter', {
  effect: 'overdrive',
  parameter: 'drive',
  value: 0.75
})
```

**getDevices**
```typescript
const devices = await ipcRenderer.invoke('getDevices')
// Returns: Array<{id: string, name: string, isInput: boolean}>
```

**setDevice**
```typescript
await ipcRenderer.invoke('setDevice', {
  deviceId: 'ASIO:UR22mkII',
  isInput: true
})
```

**savePreset**
```typescript
await ipcRenderer.invoke('savePreset', {
  name: 'Crunch Blues',
  data: {effects: {...}}
})
```

---

### Main → Renderer (IPC Send)

**meterUpdate** (continuous)
```typescript
ipcRenderer.send('meterUpdate', {
  inputLevel: -6.5,
  outputLevel: -3.2,
  peaks: [-1.2, -0.8, -2.1]
})
```

**parameterChanged**
```typescript
ipcRenderer.send('parameterChanged', {
  effect: 'overdrive',
  parameter: 'drive',
  value: 0.75
})
```

**error**
```typescript
ipcRenderer.send('error', {
  code: 'DEVICE_NOT_FOUND',
  message: 'Audio device not available'
})
```

---

## Success Criteria

### Day 1-3 (Electron Setup)
- ✓ Electron app launches with blank window
- ✓ IPC bridge working (test message round-trip)
- ✓ React frontend loads in Electron window
- ✓ npm dev/build scripts work

### Day 4-8 (Native Module)
- ✓ Native module compiles without errors
- ✓ JavaScript can call C++ functions
- ✓ Audio initialization works
- ✓ Parameter changes reflected in audio
- ✓ Meter data flows to JavaScript

### Day 9-11 (Refactoring)
- ✓ All components use IPC (not HTTP)
- ✓ Debouncing working (50ms batches)
- ✓ No HTTP traffic observed
- ✓ WebServer completely removed

### Day 12-13 (Integration)
- ✓ Knob → IPC → Native → Audio → Meter works
- ✓ Device switching works
- ✓ Preset save/load works
- ✓ No memory leaks detected
- ✓ Performance < 50ms latency

### Day 14-15 (Testing & Release)
- ✓ E2E tests all passing
- ✓ Load test: 500+ req/sec stable
- ✓ v0.9.0 released on GitHub
- ✓ Windows executable available
- ✓ Documentation updated

---

## Known Challenges

### Challenge 1: Node-gyp Setup
**Problem**: node-gyp requires Visual Studio C++ build tools, which can be finicky on Windows
**Solution**: Pre-create node-gyp binding template, document setup steps clearly

### Challenge 2: JUCE in Native Module
**Problem**: JUCE library integration with node-gyp (different build systems)
**Solution**: Either:
  - Keep JUCE static lib, link in native module, or
  - Extract audio processing into pure C++ without JUCE UI dependencies

### Challenge 3: IPC Performance
**Problem**: IPC might be slower than HTTP for meter streaming
**Solution**: Batch meter updates (10ms interval instead of per-sample), use ArrayBuffer for efficient transfer

### Challenge 4: Backward Compatibility
**Problem**: v0.8.0 JUCE version vs v0.9.0 Electron version
**Solution**: Keep v0.8.0 branch, document migration path, create hybrid installer if needed

---

## Development Setup

### Prerequisites
- Node.js 18+
- npm 9+
- Visual Studio 2022 with C++ build tools
- Python 3 (for node-gyp)
- CMake (for JUCE compilation within native module)

### Installation
```bash
npm install
npm run build-native  # Compile C++ binding
npm run dev           # Start dev server
npm run build         # Build React app
npm run dist          # Create installer
```

---

## Rollback Plan

If Electron migration fails:
1. Keep v0.8.0 as stable JUCE version
2. Branch: `feature/electron-migration` for experimental work
3. If critical issues: revert to JUCE WebBrowserComponent (no data loss, frontend same)

---

## Next Steps

1. **Approval**: Confirm architecture with user
2. **Planning**: Create detailed task breakdown
3. **Day 1 Start**: Initialize Electron project structure
4. **Daily Updates**: Track progress with task completion
5. **Mid-point Sync**: Verify native module compilation (Day 8)
6. **Final Release**: v0.9.0 on GitHub with executable

---

**Document Version**: 1.0
**Created**: 2026-06-07
**Status**: Ready for Implementation
