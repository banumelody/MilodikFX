# MilodikFX Halaman Putih Blank - Debugging Guide

**Problem**: Halaman Electron app putih blank, tidak ada UI  
**Solution**: Follow debugging steps below

---

## 🔍 Step-by-Step Debugging

### Step 1: Lihat DevTools Console

**Klik**: Ctrl+Shift+I (atau F12)

**Cek Tab Console:**
- Ada error merah? Tuliskan
- Ada warning? Tuliskan
- Apa isi errornya?

**Contoh error yang sering:**
```
Uncaught Error: Cannot find module 'react'
-> Berarti dependencies belum diinstall
```

```
Failed to load resource: net::ERR_FILE_NOT_FOUND
-> Berarti file CSS/JS missing
```

```
Preload script error
-> Berarti electron/preload.js ada masalah
```

---

### Step 2: Lihat DevTools Network

**Buka tab**: Network  
**Lalu**: Refresh page (F5)  
**Cek apakah file berhasil load:**

✅ Success (status 200):
- index.html
- *.css files
- *.js files

❌ Failed (status 404 merah):
- Berarti file tidak ada atau path salah

---

### Step 3: Lihat DevTools Elements

**Buka tab**: Elements (atau Inspector)

**Cek:**
- Ada struktur HTML?
- Atau cuma blank?
- Ada `<div id="root"></div>` atau similar?

---

## 🛠️ Common Fixes

### Fix 1: React Build Tidak Generate

**Problem**: Halaman blank karena frontend/dist/ kosong

**Solution:**
```bash
cd frontend
npm install
npm run build
cd ..
npm run dist:win
```

---

### Fix 2: Assets Path Salah

**Problem**: File CSS/JS not found (404)

**Solution**: Check electron/main.js
```javascript
// Pastikan path benar:
const prodPath = path.join(__dirname, '../frontend/dist/index.html');
// NOT: path.join(__dirname, 'frontend/dist/index.html');
```

---

### Fix 3: Dependencies Missing

**Problem**: Error "Cannot find module"

**Solution:**
```bash
# Root
npm install

# Frontend
cd frontend
npm install
cd ..

# Rebuild
npm run dist:win
```

---

### Fix 4: Preload Script Error

**Problem**: "Preload script error" di console

**Solution**: Check electron/preload.js
```javascript
// Make sure contextBridge is used:
const { contextBridge } = require('electron');

contextBridge.exposeInMainWorld('audioEngine', {
  // ... API methods
});
```

---

## 🚀 Fallback: Use Dev Mode Instead

Jika production masih blank, gunakan **dev mode**:

**Terminal 1 - React Dev Server:**
```bash
cd frontend
npm run dev
# Akan running di localhost:3000
```

**Terminal 2 - Electron:**
```bash
npm run dev:electron
# Akan load dari localhost:3000
```

**Ini akan:**
- ✅ Load React from dev server (localhost:3000)
- ✅ Open DevTools automatically
- ✅ Hot module reload saat ada perubahan
- ✅ Lebih mudah debug

---

## 📋 What to Report

Jika masih ada masalah, report dengan:

1. **Screenshot DevTools Console** - copy error messagenya
2. **DevTools Network tab** - lihat file mana yang 404
3. **Output dari npm run dist:win** - apakah ada build error?
4. **Output dari Electron startup** - apakah ada log?

---

## ✅ Success Signs

Ketika sudah bekerja, yang kita lihat:

```
✅ Halaman tidak blank - ada dark theme background
✅ Device selector dropdown visible
✅ Parameter controls (sliders) visible
✅ Audio meter visualization visible
✅ Buttons are clickable
✅ DevTools shows no red errors
```

---

## 🔗 File References

- **electron/main.js** - Electron main process, path configuration
- **electron/preload.js** - IPC bridge
- **frontend/dist/index.html** - Production build entry
- **package.json** - npm scripts and dependencies
- **troubleshoot.js** - Run: `node troubleshoot.js`

---

## 💬 Debugging Checklist

- [ ] DevTools open (Ctrl+Shift+I)
- [ ] Console tab checked - any red errors?
- [ ] Network tab checked - all files loaded (200)?
- [ ] Elements tab checked - see HTML structure?
- [ ] frontend/dist/ folder exists?
- [ ] npm install done in root and frontend/?
- [ ] npm run build completed without errors?
- [ ] npm run dist:win completed without errors?

---

## 🎯 Next Action

1. Follow steps above
2. Check DevTools
3. Report what you see
4. Based on error, we'll apply specific fix
5. Rebuild and test again

---

**Goal**: Get from blank white page → Full working UI with controls and meters 🚀
