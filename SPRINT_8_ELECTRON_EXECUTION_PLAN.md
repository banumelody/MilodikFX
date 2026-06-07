# Sprint 8: Electron Migration - Detailed Execution Plan

## Phase 1: Electron Setup (Days 1-3)

### Day 1: Initialize Electron Project

#### Task 1.1: Create Electron Project Structure
**Time**: 2 hours
**Status**: Not started

```bash
# Create new Electron project from scratch
mkdir electron-app
cd electron-app
npm init -y
npm install electron --save-dev
npm install electron-builder --save-dev
```

**Deliverable**: Basic Electron project scaffold

**Files to create**:
1. `main.js` - Electron main process entry point
2. `preload.js` - IPC bridge security layer
3. `public/index.html` - Entry HTML for Electron window
4. `package.json` - Updated with Electron scripts

**main.js template**:
```javascript
const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');

let mainWindow;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1400,
    height: 900,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      nodeIntegration: false,
      contextIsolation: true,
      enableRemoteModule: false
    }
  });

  if (process.env.NODE_ENV === 'development') {
    mainWindow.loadURL('http://localhost:3000');
    mainWindow.webContents.openDevTools();
  } else {
    mainWindow.loadFile('dist/index.html');
  }
}

app.on('ready', createWindow);
app.on('window-all-closed', () => process.exit(0));

// IPC handlers will be added here
```

#### Task 1.2: Setup IPC Preload Bridge
**Time**: 1 hour
**Status**: Not started

**preload.js template**:
```javascript
const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('ipcRenderer', {
  invoke: (channel, data) => ipcRenderer.invoke(channel, data),
  on: (channel, callback) => ipcRenderer.on(channel, callback),
  send: (channel, data) => ipcRenderer.send(channel, data),
  removeAllListeners: (channel) => ipcRenderer.removeAllListeners(channel)
});

contextBridge.exposeInMainWorld('audioEngine', {
  setParameter: (effect, param, value) => 
    ipcRenderer.invoke('setParameter', {effect, param, value}),
  getDevices: () => 
    ipcRenderer.invoke('getDevices'),
  setDevice: (deviceId) => 
    ipcRenderer.invoke('setDevice', {deviceId}),
  onMeterUpdate: (callback) => {
    ipcRenderer.removeAllListeners('meterUpdate');
    ipcRenderer.on('meterUpdate', (event, data) => callback(data));
  },
  savePreset: (name, data) => 
    ipcRenderer.invoke('savePreset', {name, data}),
  loadPreset: (id) => 
    ipcRenderer.invoke('loadPreset', {id})
});
```

**Deliverable**: Secure IPC bridge ready for React components

#### Task 1.3: Setup package.json Scripts
**Time**: 30 minutes
**Status**: Not started

**Updated package.json**:
```json
{
  "name": "milodikfx",
  "version": "0.9.0",
  "description": "Modern audio DSP editor",
  "main": "main.js",
  "homepage": "./",
  "scripts": {
    "react-start": "react-scripts start",
    "react-build": "react-scripts build",
    "react-eject": "react-scripts eject",
    "electron": "electron .",
    "dev": "concurrently npm:react-start npm:electron",
    "build-react": "npm run react-build",
    "build-native": "node-gyp configure && node-gyp build",
    "build": "npm run build-react && npm run build-native",
    "dist": "npm run build && electron-builder",
    "start": "npm run dev"
  },
  "dependencies": {
    "react": "^18.2.0",
    "react-dom": "^18.2.0",
    "zustand": "^4.3.0",
    "lucide-react": "^latest",
    "tailwindcss": "^3.0.0"
  },
  "devDependencies": {
    "electron": "^27.0.0",
    "electron-builder": "^24.0.0",
    "node-gyp": "^9.3.0",
    "react-scripts": "5.0.1",
    "concurrently": "^8.0.0"
  },
  "build": {
    "appId": "com.milodikfx.app",
    "productName": "MilodikFX",
    "files": [
      "dist/**/*",
      "main.js",
      "preload.js",
      "node_modules/**/*"
    ],
    "win": {
      "target": ["nsis"],
      "certificateFile": null,
      "signingHashAlgorithms": ["sha256"]
    }
  }
}
```

**Deliverable**: npm scripts configured for dev, build, and distribution

---

### Day 2: IPC Communication Setup

#### Task 2.1: Implement IPC Message Types & Handlers
**Time**: 3 hours
**Status**: Not started

**Create**: `src/electron/ipc-handlers.js`

```javascript
const { ipcMain } = require('electron');
let audioEngine = null; // Will be loaded from native module

// Initialize audio engine with native module
function initializeAudioEngine() {
  try {
    audioEngine = require('../native/build/Release/audio_binding');
    audioEngine.init();
    return true;
  } catch (error) {
    console.error('Failed to load native audio module:', error);
    return false;
  }
}

// Parameter debouncing
let parameterQueue = [];
let debounceTimer = null;

function flushParameters() {
  if (parameterQueue.length > 0) {
    audioEngine.setParameters(parameterQueue);
    parameterQueue = [];
  }
  debounceTimer = null;
}

// IPC Handlers
ipcMain.handle('setParameter', async (event, {effect, parameter, value}) => {
  parameterQueue.push({effect, parameter, value});
  
  clearTimeout(debounceTimer);
  debounceTimer = setTimeout(flushParameters, 50); // 50ms debounce
  
  return {success: true};
});

ipcMain.handle('getParameter', async (event, {effect, parameter}) => {
  const value = audioEngine.getParameter(effect, parameter);
  return {value};
});

ipcMain.handle('getDevices', async (event) => {
  const devices = audioEngine.getDeviceList();
  return devices;
});

ipcMain.handle('setDevice', async (event, {deviceId, isInput}) => {
  audioEngine.setDevice(deviceId, isInput);
  return {success: true};
});

ipcMain.handle('savePreset', async (event, {name, data}) => {
  const id = audioEngine.savePreset(name, JSON.stringify(data));
  return {success: true, id};
});

ipcMain.handle('loadPreset', async (event, {id}) => {
  const data = audioEngine.loadPreset(id);
  return {success: true, data: JSON.parse(data)};
});

// Periodic meter update broadcast (every 10ms)
setInterval(() => {
  const meterData = audioEngine.getMeterData();
  // Broadcast to all windows
  require('electron').BrowserWindow.getAllWindows().forEach(window => {
    window.webContents.send('meterUpdate', meterData);
  });
}, 10);

module.exports = { initializeAudioEngine };
```

**Deliverable**: All IPC handlers implemented and debouncing logic ready

#### Task 2.2: Update main.js to Load IPC Handlers
**Time**: 1 hour
**Status**: Not started

Integrate IPC handlers into main.js:
```javascript
const { initializeAudioEngine } = require('./src/electron/ipc-handlers');

function createWindow() {
  // ... existing code ...
  
  // Initialize audio engine after window created
  if (!initializeAudioEngine()) {
    console.warn('Audio engine initialization failed');
  }
}
```

**Deliverable**: main.js integrated with IPC handlers

#### Task 2.3: Test IPC Communication
**Time**: 1 hour
**Status**: Not started

Create test file: `src/electron/ipc-test.js`
- Test setParameter message round-trip
- Test getDevices message
- Test meterUpdate broadcast
- Verify no message corruption

Run:
```bash
npm run electron
# In DevTools console:
# window.ipcRenderer.invoke('getDevices')
# Should return array of devices
```

**Deliverable**: IPC communication verified working

---

### Day 3: Build Configuration & First Launch

#### Task 3.1: Setup React App in Electron Window
**Time**: 2 hours
**Status**: Not started

Modify React app entry point to work in Electron:

**src/index.js**:
```javascript
import React from 'react';
import ReactDOM from 'react-dom/client';
import App from './App';
import './index.css';

const root = ReactDOM.createRoot(document.getElementById('root'));
root.render(<App />);

// Prevent default zoom behavior
window.addEventListener('wheel', (e) => {
  if (e.ctrlKey) e.preventDefault();
});
```

**public/index.html** (Electron version):
```html
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>MilodikFX</title>
  </head>
  <body style="margin: 0; overflow: hidden;">
    <div id="root"></div>
    <script src="../src/index.js"></script>
  </body>
</html>
```

**Deliverable**: React app renders in Electron window

#### Task 3.2: Setup Development Server
**Time**: 1 hour
**Status**: Not started

Create dev startup script:

**scripts/dev.js**:
```javascript
const { spawn } = require('child_process');
const path = require('path');

console.log('Starting dev environment...');

// Start React dev server
const react = spawn('npm', ['run', 'react-start'], {
  stdio: 'inherit',
  env: {...process.env, NODE_ENV: 'development'}
});

// Wait for React server to start, then start Electron
setTimeout(() => {
  const electron = spawn('npm', ['run', 'electron'], {
    stdio: 'inherit',
    env: {...process.env, NODE_ENV: 'development'}
  });
}, 3000);

// Clean up on exit
process.on('exit', () => {
  react.kill();
  electron.kill();
});
```

**Deliverable**: `npm run dev` starts both React and Electron

#### Task 3.3: First Full Launch Test
**Time**: 1 hour
**Status**: Not started

```bash
npm install
npm run build-native  # Prepare native module stub
npm run dev
```

**Expected result**:
- Electron window opens
- React app loads in Electron window
- DevTools available (Ctrl+Shift+I)
- IPC communication works

**Deliverable**: Electron + React running together

---

## Phase 2: C++ Native Module (Days 4-8)

### Day 4: Node-gyp & Build Setup

#### Task 4.1: Initialize Node-gyp Project
**Time**: 2 hours
**Status**: Not started

Create: `src/native/binding.gyp`

```python
{
  "targets": [
    {
      "target_name": "audio_binding",
      "sources": [
        "src/binding.cc",
        "src/audio_engine.cc",
        "src/audio_engine_wrapper.cc"
      ],
      "include_dirs": [
        "<!(node -p \"require('node-addon-api').include_dir\")",
        "../../juce/modules",
        "../.."
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-fno-exceptions"],
      "conditions": [
        [
          "OS == 'win'",
          {
            "msbuild_settings": {
              "VCCLCompilerTool": {
                "ExceptionHandling": 1
              }
            },
            "libraries": [
              "ws2_32.lib",
              "winmm.lib",
              "ole32.lib"
            ]
          }
        ]
      ]
    }
  ]
}
```

**Deliverable**: binding.gyp configured for Windows

#### Task 4.2: Setup Visual Studio Build Environment
**Time**: 1 hour
**Status**: Not started

```bash
npm install --global windows-build-tools  # One-time setup
# OR manually ensure:
# - Python 3.x installed
# - Visual Studio 2022 with C++ build tools
# - node-gyp configured
```

Run:
```bash
npm run build-native
# Should compile without errors
```

**Deliverable**: Native module compilation working

#### Task 4.3: Create Hello-World Binding
**Time**: 1 hour
**Status**: Not started

**src/native/src/binding.cc** (skeleton):

```cpp
#include <napi.h>

Napi::String HelloWorld(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  return Napi::String::New(env, "Hello from C++!");
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set(Napi::String::New(env, "helloWorld"), 
              Napi::Function::New(env, HelloWorld));
  return exports;
}

NODE_API_MODULE(audio_binding, Init)
```

Test:
```javascript
const binding = require('./build/Release/audio_binding');
console.log(binding.helloWorld()); // "Hello from C++!"
```

**Deliverable**: Native module compiles and runs basic code

---

### Day 5: JUCE Audio Engine Extraction

#### Task 5.1: Extract Core Audio Processing
**Time**: 3 hours
**Status**: Not started

From existing v0.8.0 code, extract and refactor:
- AudioDeviceManager initialization
- DSP processor chain (Gain, Overdrive, EQ, etc.)
- Parameter handling (lock-free atomic updates)
- Meter data collection

Create: **src/native/src/audio_engine_wrapper.h**

```cpp
#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>
#include <map>

class AudioEngineWrapper {
public:
  AudioEngineWrapper();
  ~AudioEngineWrapper();
  
  // Initialization
  bool initialize();
  bool shutdown();
  
  // Parameter control
  void setParameter(const std::string& effect, 
                   const std::string& parameter, 
                   float value);
  float getParameter(const std::string& effect, 
                    const std::string& parameter);
  
  // Device management
  juce::StringArray getDeviceList();
  void setDevice(const std::string& deviceId);
  
  // Meter data
  struct MeterData {
    float inputLevel;
    float outputLevel;
    float peakLeft;
    float peakRight;
  };
  MeterData getMeterData() const;
  
  // Preset management
  std::string savePreset(const std::string& name, const std::string& data);
  std::string loadPreset(const std::string& id);
  
private:
  // Audio processor chain
  juce::AudioDeviceManager deviceManager;
  // DSP processors here
  
  // Thread-safe meter data
  mutable std::atomic<float> inputLevel {0.0f};
  mutable std::atomic<float> outputLevel {0.0f};
};
```

**Deliverable**: AudioEngineWrapper class skeleton ready for JUCE integration

#### Task 5.2: Implement NAPI Bindings
**Time**: 2 hours
**Status**: Not started

**src/native/src/binding.cc** (complete):

```cpp
#include <napi.h>
#include "audio_engine_wrapper.h"

static AudioEngineWrapper* gAudioEngine = nullptr;

// JavaScript: audioEngine.init()
Napi::Boolean Initialize(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!gAudioEngine) {
    gAudioEngine = new AudioEngineWrapper();
  }
  bool success = gAudioEngine->initialize();
  return Napi::Boolean::New(env, success);
}

// JavaScript: audioEngine.setParameter('overdrive', 'drive', 0.75)
void SetParameter(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (info.Length() != 3) {
    Napi::TypeError::New(env, "Expected 3 arguments").ThrowAsJavaScriptException();
    return;
  }
  
  std::string effect = info[0].As<Napi::String>();
  std::string parameter = info[1].As<Napi::String>();
  float value = info[2].As<Napi::Number>();
  
  if (gAudioEngine) {
    gAudioEngine->setParameter(effect, parameter, value);
  }
}

// JavaScript: audioEngine.getMeterData()
Napi::Object GetMeterData(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  
  if (!gAudioEngine) {
    return Napi::Object::New(env);
  }
  
  auto meters = gAudioEngine->getMeterData();
  Napi::Object obj = Napi::Object::New(env);
  obj.Set("inputLevel", Napi::Number::New(env, meters.inputLevel));
  obj.Set("outputLevel", Napi::Number::New(env, meters.outputLevel));
  obj.Set("peakLeft", Napi::Number::New(env, meters.peakLeft));
  obj.Set("peakRight", Napi::Number::New(env, meters.peakRight));
  
  return obj;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("initialize", Napi::Function::New(env, Initialize));
  exports.Set("setParameter", Napi::Function::New(env, SetParameter));
  exports.Set("getMeterData", Napi::Function::New(env, GetMeterData));
  // ... other functions
  return exports;
}

NODE_API_MODULE(audio_binding, Init)
```

**Deliverable**: NAPI bindings for audio engine functions

#### Task 5.3: Test Native Module with React
**Time**: 1 hour
**Status**: Not started

In React component, test native module:

```typescript
// src/components/AudioTest.tsx
import React, { useEffect } from 'react';

export const AudioTest = () => {
  useEffect(() => {
    const testAudioEngine = async () => {
      // Call native module through IPC
      const result = await window.ipcRenderer.invoke('initAudio');
      console.log('Audio init result:', result);
      
      const meterData = await window.ipcRenderer.invoke('getMeterData');
      console.log('Meter data:', meterData);
    };
    
    testAudioEngine();
  }, []);
  
  return <div>Audio engine test running...</div>;
};
```

**Deliverable**: Native module integrated with React

---

### Day 6-7: Meter Data Streaming & Device Management

(Continues similar structure for meter streaming, device enumeration, etc.)

---

### Day 8: Native Module Testing & Optimization

#### Task 8.1: Comprehensive Testing
- Unit tests for native module
- Load testing (100+ parameter updates/sec)
- Memory profiling
- Meter latency measurements

#### Task 8.2: Performance Optimization
- Optimize meter data transfer (batch updates)
- Optimize parameter queue processing
- Profile memory usage
- Add memory cleanup

**Deliverable**: Native module production-ready

---

## Phase 3: Communication Refactoring (Days 9-11)

### Day 9: Remove HTTP, Setup IPC-Only

#### Task 9.1: Remove WebServer
- Delete WebServer.h/cpp
- Remove WebServer initialization from MainComponent
- Remove localhost:3000 startup code
- Update CMakeLists.txt

#### Task 9.2: Verify No HTTP Dependencies
```bash
grep -r "localhost" src/
grep -r "http://" src/
grep -r "WebServer" src/
# All should return nothing
```

---

### Day 10: Refactor Components to Use IPC

#### Components to Update:
1. **Knob.tsx**
   - Before: `fetch('/api/parameters/setParameter', ...)`
   - After: `window.audioEngine.setParameter(...)`

2. **Meter.tsx**
   - Before: Polling `/api/meters`
   - After: `window.audioEngine.onMeterUpdate(...)`

3. **DeviceSelector.tsx**
   - Before: `fetch('/api/devices')`
   - After: `await window.ipcRenderer.invoke('getDevices')`

---

### Day 11: Main Process IPC Optimization

- Implement debouncing in main process
- Add error handling for all IPC calls
- Add logging for debugging
- Test error scenarios

---

## Phase 4: Integration & Polish (Days 12-13)

### Day 12: Full Integration Test
- Test parameter flow: Knob → IPC → Native → DSP → Audio
- Test meter visualization
- Test device switching
- Test preset save/load
- Memory leak detection

### Day 13: Bug Fixes
- Fix any issues discovered in Day 12
- Optimize startup time
- Polish UI
- Update documentation

---

## Phase 5: Release & Testing (Days 14-15)

### Day 14: E2E & Load Testing
- Automated E2E tests
- Load testing (500+ param/sec)
- Performance benchmarking
- Cross-version testing

### Day 15: Release
- Build release executable
- Create v0.9.0 release notes
- Publish GitHub Release
- Document Electron architecture
- Create migration guide from v0.8.0

---

## Daily Status Tracking Template

```markdown
### Day N Status
**Date**: YYYY-MM-DD
**Tasks Completed**:
- [ ] Task 1
- [ ] Task 2

**Issues Encountered**:
- Issue 1: Description and resolution

**Next Day Plan**:
- Task list for tomorrow

**Metrics**:
- Build time: X seconds
- Native module size: X MB
- App startup: X seconds
```

---

## Rollback Checkpoints

- **End of Day 3**: Electron + React working, revert if IPC issues
- **End of Day 8**: Native module working, revert if compilation issues
- **End of Day 11**: IPC refactoring complete, revert if communication fails
- **End of Day 13**: Full integration working, ready for release

---

**Document Version**: 1.0
**Created**: 2026-06-07
**Last Updated**: 2026-06-07
**Status**: Ready for Implementation
