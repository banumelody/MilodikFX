# Single Executable Bundle Strategy - MilodikFX v0.8.0

## Goal: Create ONE .exe yang berisi Backend + Frontend

---

## APPROACH: Embedded Web View dalam JUCE App

### Architecture
```
MilodikFX.exe (Single executable)
├── C++ Audio Engine (JUCE)
├── Embedded Web Server (port 9999)
├── React Frontend (bundled)
└── IPC Bridge (internal communication)

User downloads ONE .exe → Double-click → Everything works!
```

---

## Implementation Steps

### 1. Embed Frontend into Backend Binary

**Copy production build**:
```
frontend/dist/ → src/ui/assets/
```
Files:
- index.html
- assets/index-*.js
- assets/index-*.css

**Update CMakeLists.txt**:
```cmake
# Include frontend assets as binary resources
juce_add_binary_data(UIAssets
    SOURCES
        src/ui/assets/index.html
        src/ui/assets/index-*.js
        src/ui/assets/index-*.css
)

target_link_libraries(MilodikFX PRIVATE UIAssets)
```

### 2. Add Simple HTTP Server in C++

**Create src/ui/WebServer.h/cpp**:
```cpp
class WebServer {
public:
    WebServer(int port = 3000);
    ~WebServer();
    
    bool start();
    void stop();
    bool isRunning() const;
    
    // Serve embedded HTML/JS/CSS
    void handleRequest(const std::string& path, std::string& response);
    
private:
    std::unique_ptr<asio::io_context> io_;
    std::unique_ptr<tcp::acceptor> acceptor_;
    int port_;
    bool running_;
};
```

### 3. Integrate into Main Component

**MainComponent.cpp**:
```cpp
MainComponent::MainComponent() {
    webServer_ = std::make_unique<WebServer>(3000);
    webServer_->start();
    
    // Launch browser to http://localhost:3000
    juce::URL("http://localhost:3000").launchInDefaultBrowser();
    
    // Keep JUCE window active
}
```

### 4. Build Single Executable

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release

# Output: MilodikFX.exe (single file)
```

---

## Simpler Alternative: Electron (Recommended for this project)

### Why Electron?
- ✅ Cross-platform (Windows/Mac/Linux)
- ✅ Easy packaging
- ✅ Auto-update support
- ✅ Native .exe/.dmg/.deb distribution
- ✅ Better web tech integration
- ✅ Smaller executable than embedded web view

### Architecture
```
Electron App
├── Preload script (ipc bridge)
├── Main process (backend .exe as child process)
└── Renderer (React frontend)
```

---

## RECOMMENDATION: Hybrid Approach

**Best of both worlds**:
1. Keep JUCE backend as separate .exe (audio processing)
2. Build Electron wrapper for frontend
3. Electron launcher starts backend .exe + opens renderer
4. Users get: `MilodikFX.exe` (Electron wrapper)

---

## What I'll Do

**Option 1**: Embed frontend in JUCE (simplest, single executable)
```
Pros:
- Single .exe
- No dependencies
- Fast startup
- Everything standalone

Cons:
- More C++ work
- Browser window is embedded
- No auto-update
```

**Option 2**: Electron wrapper (recommended)
```
Pros:
- Professional desktop app
- Easy to maintain
- Cross-platform ready
- Auto-update support
- Better UX

Cons:
- Larger .exe (~100MB)
- Requires Node.js build
- Two processes
```

---

## Which should I implement?

Tell me:
1. **Single embedded .exe** (JUCE approach) - simpler, smaller
2. **Electron wrapper** (professional approach) - larger, feature-rich
3. **Two-file installer** (.exe + launcher) - compromise

---

