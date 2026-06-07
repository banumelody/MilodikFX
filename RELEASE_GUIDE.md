# MilodikFX v0.9.0 Windows Release Guide

## 📦 Release Artifacts

### Location
`D:\Projects\MilodikFX\dist\`

### Files

#### 1. MilodikFX Setup 0.9.0-electron.exe (71.16 MB)
**Type**: Installer
**Use Case**: End users - traditional software installation experience

**Installation**:
```
1. Double-click MilodikFX Setup 0.9.0-electron.exe
2. Follow installer wizard
3. Choose installation directory (default: C:\Users\{user}\AppData\Local\Programs\MilodikFX)
4. Installer creates Start Menu shortcuts
5. App ready to launch
```

**Uninstall**:
- Control Panel → Programs → Programs and Features → MilodikFX → Uninstall
- Or: C:\Users\{user}\AppData\Local\Programs\MilodikFX\Uninstall MilodikFX.exe

#### 2. MilodikFX 0.9.0-electron.exe (70.94 MB)
**Type**: Portable (Standalone)
**Use Case**: USB drives, testing, no installation required

**Execution**:
```
1. Copy MilodikFX 0.9.0-electron.exe to desired location
2. Double-click to launch
3. No installation needed
4. No Start Menu shortcuts created
```

**Portability**:
- Can be run from USB drive
- All files self-contained in .exe
- Can be moved between machines
- No registry entries

## 🚀 Launching the Application

### From Installer
1. Start Menu → MilodikFX
2. Or: Double-click desktop shortcut (if created)

### From Portable
1. Double-click: MilodikFX 0.9.0-electron.exe
2. Or: Command line: `.\MilodikFX\ 0.9.0-electron.exe`

### Expected Behavior
1. Electron window opens (~2-3 seconds)
2. React frontend loads
3. Dark theme UI visible
4. Device selector populated
5. Audio meters visible (mock data)

## 🧪 Testing Checklist

### Window & UI
- [ ] Window opens without errors
- [ ] UI renders correctly (dark theme)
- [ ] All text readable
- [ ] Buttons responsive
- [ ] No layout issues

### Device Panel
- [ ] Device dropdown shows items
- [ ] Can select device
- [ ] Selection persists

### Parameter Controls
- [ ] Can adjust sliders
- [ ] Parameter values update
- [ ] Knobs respond to clicks

### Meters
- [ ] Meter visualization visible
- [ ] Levels display (even if mock data)
- [ ] No crashes during visualization

### Developer Features
- [ ] Right-click → Inspect opens DevTools
- [ ] Console shows messages
- [ ] Can refresh with F5

## 🔧 Troubleshooting

### App Won't Launch
- **Check**: Windows 10 or later required
- **Check**: x64 architecture (Windows 11/10 Pro typically OK)
- **Try**: Run as Administrator
- **Try**: Check Windows Defender/Antivirus (might be blocking)

### App Crashes on Startup
- **Check**: System requirements (Windows 10+, 4GB+ RAM)
- **Try**: Delete `%APPDATA%\MilodikFX` folder for fresh start
- **Try**: Reinstall using installer

### DevTools Not Opening
- **Try**: Right-click → Inspect again
- **Try**: F12 key
- **Try**: Ctrl+Shift+I

### UI Not Rendering
- **Check**: GPU drivers updated (Windows Update)
- **Try**: Restart application

## 📊 System Requirements

### Minimum
- **OS**: Windows 10 (build 19041) or Windows 11
- **Architecture**: x64 (64-bit)
- **RAM**: 2 GB
- **Storage**: 200 MB free

### Recommended
- **OS**: Windows 11 latest build
- **Architecture**: x64
- **RAM**: 4 GB
- **Storage**: 500 MB free
- **GPU**: Discrete GPU recommended for smooth UI rendering

## 📝 Version Info

- **Version**: 0.9.0-electron
- **Type**: MVP Release (Electron Migration)
- **Audio Engine**: Mock (placeholder for production)
- **Frontend**: React + TailwindCSS
- **Container**: Electron 27.3.11
- **Release Date**: 2024

## 🔐 Security Notes

- Code not signed (self-signed installation only)
- No telemetry or usage tracking
- All processing local (no cloud components)
- Native module: Compiled from source (open-source on GitHub)

## 📞 Support

### Reporting Issues
- GitHub Issues: https://github.com/banumelody/MilodikFX/issues
- Include:
  - Windows version
  - Exact error message (if any)
  - Steps to reproduce
  - Screenshot if applicable

### Known Issues (MVP)
1. Audio engine returns mock data (not real audio)
2. Device list is simulated (not real devices)
3. No preset save/load functionality yet
4. No effect signal chain management yet

## 🎯 Next Release Plan

### Sprint 9 (Audio Focus)
- Real JUCE audio engine integration
- Real device enumeration
- Real audio processing
- Meter data from actual audio

### Sprint 10 (UI Complete)
- Preset system
- Effect signal chain management
- Add/remove effects
- Reorder effects

## 📦 Distribution Methods

### Option 1: GitHub Releases
```
1. Create Release on GitHub
2. Upload both .exe files
3. Add release notes
4. Generate download URLs for users
5. User downloads and runs
```

### Option 2: Website/Portal
```
1. Host .exe files on web server
2. Provide download link
3. Users download installer or portable
```

### Option 3: Installer Package
```
- MilodikFX Setup 0.9.0-electron.exe
  (Users run this to install with wizard)
```

### Option 4: Direct Portable Distribution
```
- MilodikFX 0.9.0-electron.exe
  (Users run directly, no installation)
```

## 🎉 Post-Release

1. **Monitor**: User feedback and issues
2. **Document**: Any problems found during testing
3. **Plan**: Sprint 9 priorities based on feedback
4. **Prepare**: Audio engine integration work

---

**Ready for Release**: ✅ Yes
**Tested**: ✅ Basic functionality
**Production Ready**: ⚠️  MVP (audio engine is mock)

For questions or issues, refer to GitHub repository or raise an issue on the project tracker.
