const { contextBridge, ipcRenderer } = require('electron');

/**
 * Preload script for Electron IPC bridge
 * Exposes safe APIs to React frontend with contextIsolation enabled
 * 
 * Usage in React:
 *   await window.audioEngine.setParameter('overdrive', 'drive', 0.75)
 *   window.audioEngine.onMeterUpdate((data) => { console.log(data) })
 */

// Expose limited IPC renderer interface
contextBridge.exposeInMainWorld('ipcRenderer', {
  invoke: (channel, data) => ipcRenderer.invoke(channel, data),
  on: (channel, callback) => ipcRenderer.on(channel, callback),
  send: (channel, data) => ipcRenderer.send(channel, data),
  removeAllListeners: (channel) => ipcRenderer.removeAllListeners(channel)
});

// Expose audio engine API (high-level interface)
contextBridge.exposeInMainWorld('audioEngine', {
  /**
   * Set parameter on an effect
   * @param {string} effect - Effect name (e.g. 'overdrive', 'eq')
   * @param {string} parameter - Parameter name (e.g. 'drive', 'tone')
   * @param {number} value - Parameter value (0.0 - 1.0)
   */
  setParameter: (effect, parameter, value) => {
    return ipcRenderer.invoke('setParameter', { effect, parameter, value });
  },

  /**
   * Get current parameter value
   * @param {string} effect - Effect name
   * @param {string} parameter - Parameter name
   * @returns {Promise<{success: boolean, value: number}>}
   */
  getParameter: (effect, parameter) => {
    return ipcRenderer.invoke('getParameter', { effect, parameter });
  },

  /**
   * Get list of available audio devices
   * @returns {Promise<{success: boolean, devices: Array}>}
   */
  getDevices: () => {
    return ipcRenderer.invoke('getDevices');
  },

  /**
   * Set active audio device
   * @param {string} deviceId - Device ID
   * @param {boolean} isInput - True for input device, false for output
   */
  setDevice: (deviceId, isInput = true) => {
    return ipcRenderer.invoke('setDevice', { deviceId, isInput });
  },

  /**
   * Save current effect parameters as a preset
   * @param {string} name - Preset name
   * @param {object} data - Preset data (effect parameters)
   * @returns {Promise<{success: boolean, id: string}>}
   */
  savePreset: (name, data) => {
    return ipcRenderer.invoke('savePreset', { name, data });
  },

  /**
   * Load a saved preset
   * @param {string} id - Preset ID
   * @returns {Promise<{success: boolean, data: object}>}
   */
  loadPreset: (id) => {
    return ipcRenderer.invoke('loadPreset', { id });
  },

  /**
   * Subscribe to meter updates
   * @param {Function} callback - Called with meter data on each update
   * Meter data format:
   * {
   *   inputLevel: number,      // dB (-60 to 0)
   *   outputLevel: number,     // dB (-60 to 0)
   *   peakLeft: number,        // dB (-60 to 0)
   *   peakRight: number        // dB (-60 to 0)
   * }
   */
  onMeterUpdate: (callback) => {
    // Remove existing listeners to prevent duplicates
    ipcRenderer.removeAllListeners('meterUpdate');
    // Setup new listener
    ipcRenderer.on('meterUpdate', (event, data) => {
      callback(data);
    });
  },

  /**
   * Unsubscribe from meter updates
   */
  offMeterUpdate: () => {
    ipcRenderer.removeAllListeners('meterUpdate');
  }
});

console.log('[Preload] IPC bridge initialized');
