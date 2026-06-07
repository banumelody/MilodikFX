const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const isDev = require('electron-is-dev');

let mainWindow;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1400,
    height: 900,
    minWidth: 1024,
    minHeight: 600,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      nodeIntegration: false,
      contextIsolation: true,
      enableRemoteModule: false,
      sandbox: true
    },
    icon: path.join(__dirname, 'assets', 'icon.png')
  });

  // Load React app
  if (isDev) {
    // Development: Load from Vite dev server on localhost:3000
    mainWindow.loadURL('http://localhost:3000');
    mainWindow.webContents.openDevTools();
    console.log('[Main] Loading React dev server from http://localhost:3000');
  } else {
    // Production: Load from built files in frontend/dist
    // __dirname is electron/ folder, so we need to go up to root, then into frontend/dist
    const prodPath = path.join(__dirname, '../frontend/dist/index.html');
    
    // Verify file exists before loading
    const fs = require('fs');
    if (fs.existsSync(prodPath)) {
      mainWindow.loadFile(prodPath);
      console.log('[Main] Loading production build from:', prodPath);
      // Enable DevTools for production debugging
      mainWindow.webContents.openDevTools();
    } else {
      console.error('[Main] Production build not found at:', prodPath);
      console.log('[Main] Falling back to dev server...');
      mainWindow.loadURL('http://localhost:3000');
      mainWindow.webContents.openDevTools();
    }
  }

  mainWindow.on('closed', () => {
    mainWindow = null;
  });
}

// Create window when app is ready
app.on('ready', () => {
  createWindow();
  
  // Initialize audio engine after window is created
  try {
    initializeAudioEngine();
  } catch (error) {
    console.error('Failed to initialize audio engine:', error);
  }
});

// Quit app when all windows are closed (except on macOS)
app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

// Re-create window when dock icon is clicked (macOS)
app.on('activate', () => {
  if (mainWindow === null) {
    createWindow();
  }
});

// ============================================
// AUDIO ENGINE INITIALIZATION
// ============================================

let audioEngine = null;
let parameterQueue = [];
let debounceTimer = null;

function initializeAudioEngine() {
  try {
    // TODO: Load native module when ready
    // audioEngine = require('./native/build/Release/audio_binding');
    // audioEngine.init();
    console.log('[Audio] Engine initialization placeholder (waiting for native module)');
  } catch (error) {
    console.error('[Audio] Failed to load native module:', error);
  }
}

// ============================================
// IPC HANDLERS
// ============================================

// setParameter: Debounced parameter updates (50ms batching)
ipcMain.handle('setParameter', async (event, { effect, parameter, value }) => {
  parameterQueue.push({ effect, parameter, value });
  
  clearTimeout(debounceTimer);
  debounceTimer = setTimeout(() => {
    if (audioEngine) {
      audioEngine.setParameters(parameterQueue);
    } else {
      console.log('[IPC] Parameter batch (queued, no engine):', parameterQueue);
    }
    parameterQueue = [];
  }, 50);
  
  return { success: true };
});

// getParameter: Get current parameter value
ipcMain.handle('getParameter', async (event, { effect, parameter }) => {
  if (audioEngine) {
    const value = audioEngine.getParameter(effect, parameter);
    return { success: true, value };
  }
  return { success: false, value: 0 };
});

// getDevices: Get list of audio devices
ipcMain.handle('getDevices', async (event) => {
  if (audioEngine) {
    const devices = audioEngine.getDeviceList();
    return { success: true, devices };
  }
  return { 
    success: true, 
    devices: [
      { id: 'default', name: 'Default Input', isInput: true },
      { id: 'default-out', name: 'Default Output', isInput: false }
    ]
  };
});

// setDevice: Switch audio device
ipcMain.handle('setDevice', async (event, { deviceId, isInput }) => {
  if (audioEngine) {
    audioEngine.setDevice(deviceId, isInput);
    return { success: true };
  }
  console.log(`[IPC] Device change queued: ${deviceId} (input: ${isInput})`);
  return { success: true };
});

// savePreset: Save parameter preset
ipcMain.handle('savePreset', async (event, { name, data }) => {
  if (audioEngine) {
    const id = audioEngine.savePreset(name, JSON.stringify(data));
    return { success: true, id };
  }
  const id = `preset-${Date.now()}`;
  console.log(`[IPC] Preset saved (queued): ${name} -> ${id}`);
  return { success: true, id };
});

// loadPreset: Load parameter preset
ipcMain.handle('loadPreset', async (event, { id }) => {
  if (audioEngine) {
    const data = audioEngine.loadPreset(id);
    return { success: true, data: JSON.parse(data) };
  }
  console.log(`[IPC] Preset loaded (queued): ${id}`);
  return { success: true, data: {} };
});

// ============================================
// METER BROADCAST (every 10ms)
// ============================================

setInterval(() => {
  if (mainWindow && !mainWindow.isDestroyed()) {
    if (audioEngine) {
      const meterData = audioEngine.getMeterData();
      mainWindow.webContents.send('meterUpdate', meterData);
    } else {
      // Simulate meter data for testing (will be replaced by real data)
      const meterData = {
        inputLevel: Math.random() * -20,
        outputLevel: Math.random() * -20,
        peakLeft: -12 + Math.random() * 6,
        peakRight: -12 + Math.random() * 6
      };
      mainWindow.webContents.send('meterUpdate', meterData);
    }
  }
}, 10);

// ============================================
// DEBUG
// ============================================

if (isDev) {
  console.log('[Main] Electron app started in development mode');
} else {
  console.log('[Main] Electron app started in production mode');
}
