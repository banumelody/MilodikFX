const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const isDev = false; // force production behavior for local debugging (loads frontend/dist)

// Disable hardware acceleration to avoid white/blank renderer on some Windows GPU drivers
try {
  app.disableHardwareAcceleration();
} catch (e) {
  console.warn('[Main] disableHardwareAcceleration failed:', e);
}

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
    sandbox: false
    },
    backgroundColor: '#0f172a',
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
    // Try multiple candidate locations (dev vs packaged)
    const fs = require('fs');
    const candidates = [
      path.join(__dirname, '../frontend/dist/index.html'), // dev layout
      path.join(process.resourcesPath || '', 'app', 'frontend', 'dist', 'index.html'), // packaged (app/frontend/...)
      path.join(process.resourcesPath || '', 'frontend', 'dist', 'index.html') // packaged alternative
    ];
    let prodPath = candidates.find(p => p && fs.existsSync(p));

    // If none found, fall back to default relative path (helps during debugging)
    if (!prodPath) prodPath = path.join(__dirname, '../frontend/dist/index.html');

    // Verify file exists before loading
    if (fs.existsSync(prodPath)) {
      // Write the index.html contents to debug folder before loading to ensure we're loading the right file
      try {
        const htmlContent = fs.readFileSync(prodPath, 'utf8');
        const dbgDir = path.join(app.getPath('userData'), 'milodikfx-debug');
        if (!fs.existsSync(dbgDir)) fs.mkdirSync(dbgDir, { recursive: true });
        fs.writeFileSync(path.join(dbgDir, 'index_html_snapshot.txt'), htmlContent, 'utf8');
        console.log('[Main] Wrote index.html snapshot to debug folder');
      } catch (err) {
        console.error('[Main] Failed to snapshot index.html:', err);
      }

      mainWindow.loadFile(prodPath);
      console.log('[Main] Loading production build from:', prodPath);
      // Enable DevTools for production debugging
      mainWindow.webContents.openDevTools();

      // Attach renderer debug listeners to capture console messages and load failures
      mainWindow.webContents.on('console-message', (event, level, message, line, sourceId) => {
        console.log(`[Renderer Console] level=${level} ${message} (${sourceId}:${line})`);
      });

      mainWindow.webContents.on('did-fail-load', (event, errorCode, errorDescription, validatedURL, isMainFrame) => {
        console.error('[Renderer] did-fail-load', { errorCode, errorDescription, validatedURL, isMainFrame });
      });

      mainWindow.webContents.on('crashed', () => {
        console.error('[Renderer] crashed');
      });

      // After the page finishes loading, dump the DOM to a debug file for inspection
      mainWindow.webContents.on('did-finish-load', async () => {
        try {
          const html = await mainWindow.webContents.executeJavaScript('document.documentElement.outerHTML');
          const dbgDir = path.join(app.getPath('userData'), 'milodikfx-debug');
          if (!fs.existsSync(dbgDir)) fs.mkdirSync(dbgDir, { recursive: true });
          const outPath = path.join(dbgDir, `renderer_dump_${Date.now()}.html`);
          fs.writeFileSync(outPath, html, 'utf8');
          console.log('[Main] Renderer DOM dumped to', outPath);

          // Also capture the innerText of the root element to see if React mounted
          const rootText = await mainWindow.webContents.executeJavaScript('(function(){ const r = document.getElementById("root"); return r ? r.innerText : "<no-root>"; })()');
          fs.writeFileSync(path.join(dbgDir, `root_text_${Date.now()}.txt`), rootText, 'utf8');
          console.log('[Main] Root text written');
        } catch (err) {
          console.error('[Main] Failed to dump renderer DOM:', err);
        }
      });

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
