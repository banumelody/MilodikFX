# Sprint 7 Planning: Platform Migration - Frontend Foundation

**Sprint Dates:** June 7 - June 21, 2026  
**Phase:** B - Platform Migration (In Progress)  
**Target Output:** v0.8.0  
**Status:** Planning Phase

---

## Executive Summary

Sprint 7 marks the beginning of **Phase B: Platform Migration** - a strategic shift from JUCE Native UI to a modern React/TypeScript/TailwindCSS frontend architecture.

### Why This Change?

1. **Faster UI Development**: React components iterate faster than JUCE
2. **Modern UX**: Access to extensive UI libraries and design systems
3. **Signal Chain Visualization**: Web technologies excel at interactive diagrams
4. **Future Platforms**: React UI can run on web, Electron, mobile
5. **Separation of Concerns**: DSP Engine (C++) remains independent from UI Layer

### High-Level Architecture

```
┌─────────────────────────────────────────────────┐
│  React Frontend (TypeScript + TailwindCSS)      │
│  - Preset Bar                                   │
│  - Device Panel                                 │
│  - Audio Meter                                  │
│  - Effect Cards                                 │
│  - Status Bar                                   │
└────────────────┬────────────────────────────────┘
                 │ Message Bridge (IPC)
                 │
┌─────────────────────────────────────────────────┐
│  MilodikFX Engine (C++ DSP Core)                │
│  - Audio Device Management                      │
│  - Real-time DSP Processing (6 Effects)         │
│  - Preset Management                            │
│  - CPU Monitoring                               │
└─────────────────────────────────────────────────┘
```

---

## Sprint 7 Goals

### Primary Goals

1. **Establish Frontend Foundation**
   - React project setup (Vite)
   - TypeScript configuration
   - Component library infrastructure
   - Build pipeline

2. **Implement Message Bridge**
   - DSP Engine ↔ Frontend communication
   - Parameter synchronization
   - Event system

3. **Create Design System**
   - Design tokens (colors, spacing, typography)
   - Theme system (Dark/Light)
   - Component guidelines

### Success Criteria

- ✅ React + TypeScript + TailwindCSS fully configured
- ✅ Message bridge bidirectional communication working
- ✅ Basic component library with 5+ components
- ✅ Design tokens documented
- ✅ Hot reload development workflow
- ✅ Build pipeline (dev, prod, test)
- ✅ All tests passing (frontend + DSP integration)

---

## Sprint 7 Tasks

### Task 1: Frontend Foundation (v0.8.0)

**Goal:** Set up complete React development environment

**Deliverables:**
- React 18 project (Vite)
- TypeScript 5 configuration
- TailwindCSS 3 integration
- ESLint + Prettier setup
- Git workflow automation
- Development server with hot reload

**Components to Create:**
- `Knob` - Parameter slider with value display
- `Button` - Standard button component
- `Label` - Text label with tooltip support
- `Card` - Effect card container
- `Meter` - Audio level visualization

**Design Tokens:**
- Colors: Primary, Secondary, Success, Warning, Error
- Spacing: 4px base unit system
- Typography: 14px body, 18px heading, 12px small
- Dark/Light theme variants

**Deliverable Output:** v0.8.0-dev

---

### Task 2: Message Bridge & Event System (v0.8.0)

**Goal:** Establish communication between C++ DSP Engine and React Frontend

**Architecture:**
```
┌──────────────────┐
│ React Frontend   │
└────────┬─────────┘
         │ IPC Messages
         │ - GetDeviceList
         │ - SetParameter
         │ - GetPresetList
         │ - SavePreset
         │ - LoadPreset
         │
┌────────▼─────────┐
│ DSP Engine (C++) │
└──────────────────┘
```

**Message Types:**
- **Parameter Sync**: `{ type: 'parameter', processor: 'gain', param: 'gainDb', value: 12.0 }`
- **Device State**: `{ type: 'device', sampleRate: 44100, bufferSize: 512 }`
- **Preset API**: `{ type: 'preset', action: 'save|load|delete', data: {...} }`
- **CPU Load**: `{ type: 'monitor', cpuPercent: 25.5 }`

**Deliverables:**
- IPC Server (C++ side) using WebSocket or Named Pipes
- IPC Client (React side) using Socket.IO or native IPC
- Event dispatcher pattern
- Message queuing
- Error handling & recovery

**API Endpoints:**
- `GET /api/devices` - Get audio device list
- `GET /api/config` - Get current configuration
- `POST /api/parameter` - Set parameter value
- `POST /api/preset/save` - Save preset
- `GET /api/preset/load/:id` - Load preset
- `GET /api/monitor/cpu` - Get CPU load

**Deliverable Output:** v0.8.0-beta

---

### Task 3: Modern UI MVP (v1.0.0-beta)

**Goal:** Build essential UI components for device control and monitoring

**Components to Build:**

1. **Preset Bar** (~100px height)
   - Preset name display
   - Save button
   - Load button
   - Delete button
   - Recent presets dropdown

2. **Device Panel** (~80px height)
   - Audio input selector
   - Audio output selector
   - Sample rate display
   - Buffer size display
   - Latency display

3. **Audio Meter** (~120px height)
   - Input level stereo meter
   - Output level stereo meter
   - Peak indicators
   - RMS display

4. **Effect Cards** (Responsive grid)
   - Knobs for parameters
   - Toggle for enable/disable
   - Parameter labels
   - Visual feedback animations

5. **Status Bar** (~40px height)
   - CPU load percentage
   - Current theme
   - Connection status to DSP engine

**Layout:** Responsive design that adapts from 3 columns (wide) to 2 columns (narrow)

**Deliverable Output:** v1.0.0-beta

---

### Task 4: Dynamic Signal Chain (v1.0.0)

**Goal:** Implement add/remove/reorder effects UI

**Features:**
- **Add Effect**: UI to insert new effect in chain
- **Remove Effect**: Delete effect from chain
- **Reorder**: Drag & drop to change effect order
- **Signal Path**: Visual representation of signal flow
- **Enable/Disable**: Toggle effects on/off without removing

**Visual Design:**
```
┌─────────────────────────────────────────────┐
│  Signal Chain View                          │
├─────────────────────────────────────────────┤
│  [Gain] ──▶ [Overdrive] ──▶ [EQ] ──▶ [Out]  │
│                                             │
│  [+ Add Effect]  [Remove]  [Settings]      │
└─────────────────────────────────────────────┘
```

**Deliverables:**
- Effect insertion UI
- Drag & drop reordering
- Signal flow animation
- Effect bypass toggle
- Chain visualization

**Deliverable Output:** v1.0.0 (Release Candidate)

---

### Task 5: Testing & Quality Assurance

**Goal:** Ensure quality across DSP + Frontend integration

**Frontend Tests:**
- Unit tests (Jest) for React components
- Integration tests for message bridge
- UI tests (React Testing Library)
- Visual regression tests (Percy/VRT)

**DSP Integration Tests:**
- Parameter sync verification
- Preset save/load with new UI
- Audio processing with React controls
- Performance benchmarks

**Acceptance Criteria:**
- 90%+ code coverage on critical paths
- All tests passing (frontend + DSP)
- No console errors or warnings
- CPU load remains <40% with new UI
- Message latency <16ms (60 FPS)

---

## Technical Specifications

### Frontend Stack

**Framework:**
- React 18.2+
- TypeScript 5.1+
- Vite 4.3+

**Styling:**
- TailwindCSS 3.3+
- PostCSS 8+

**Build Tools:**
- ESLint 8.4+
- Prettier 2.8+
- Vitest for unit tests
- React Testing Library

**Development:**
- Node.js 18+
- npm 9+

### Project Structure

```
frontend/
├── src/
│   ├── components/
│   │   ├── Knob.tsx
│   │   ├── Button.tsx
│   │   ├── Card.tsx
│   │   ├── Meter.tsx
│   │   ├── PresetBar.tsx
│   │   ├── DevicePanel.tsx
│   │   ├── AudioMeter.tsx
│   │   ├── EffectCards.tsx
│   │   ├── StatusBar.tsx
│   │   └── SignalChain.tsx
│   ├── hooks/
│   │   ├── useAudioEngine.ts
│   │   ├── usePresets.ts
│   │   ├── useDevice.ts
│   │   └── useTheme.ts
│   ├── services/
│   │   ├── messageBridge.ts
│   │   ├── ipcClient.ts
│   │   └── eventDispatcher.ts
│   ├── themes/
│   │   ├── designTokens.ts
│   │   ├── dark.ts
│   │   └── light.ts
│   ├── App.tsx
│   └── main.tsx
├── public/
├── tests/
├── tsconfig.json
├── vite.config.ts
├── tailwind.config.js
└── package.json

backend/
├── src/
│   ├── ipc/
│   │   ├── IPCServer.h/cpp
│   │   ├── MessageHandler.h/cpp
│   │   └── EventDispatcher.h/cpp
│   └── ...
```

### Dependencies

**Frontend (package.json):**
```json
{
  "dependencies": {
    "react": "^18.2.0",
    "react-dom": "^18.2.0"
  },
  "devDependencies": {
    "typescript": "^5.1.0",
    "vite": "^4.3.0",
    "tailwindcss": "^3.3.0",
    "eslint": "^8.4.0",
    "prettier": "^2.8.0",
    "vitest": "^0.33.0",
    "@testing-library/react": "^14.0.0"
  }
}
```

---

## Implementation Plan

### Week 1 (June 7-10)

- **Day 1**: Project setup (React + TypeScript + TailwindCSS)
- **Day 2**: Design token system & theme setup
- **Day 3**: Basic component library (Knob, Button, Card, Label, Meter)
- **Day 4**: Build pipeline & dev environment

### Week 2 (June 11-17)

- **Day 1**: Message Bridge architecture & IPC implementation
- **Day 2**: Event dispatcher & parameter sync
- **Day 3**: PresetBar, DevicePanel, AudioMeter components
- **Day 4**: EffectCards and StatusBar

### Week 3 (June 18-21)

- **Day 1**: Signal chain visualization & drag-drop
- **Day 2**: Integration testing
- **Day 3**: Performance optimization
- **Day 4**: Release preparation & documentation

---

## Risk Management

### Potential Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|-----------|
| IPC latency impacts UI | Medium | High | Profile early, optimize message format |
| React bundle size | Low | Medium | Code splitting, lazy loading |
| Theming complexity | Low | Low | Use Tailwind variants system |
| Browser compatibility | Low | Low | Use standard React, test on Chromium |

---

## Dependencies & Prerequisites

### From Sprint 6
- ✅ DSP Engine (v0.7.5) - Stable, ready for integration
- ✅ Preset system (with metadata) - Ready for new UI
- ✅ CPU monitoring - Data source for UI

### New Requirements
- React development environment
- IPC/message bridge implementation
- Frontend build pipeline
- CI/CD for frontend tests

---

## Acceptance Criteria

### Task 1: Frontend Foundation
- [ ] React project created with Vite
- [ ] TypeScript fully configured
- [ ] TailwindCSS integrated with design tokens
- [ ] 5+ base components built and tested
- [ ] Hot reload development working
- [ ] Build pipeline (dev, prod) working

### Task 2: Message Bridge
- [ ] IPC server implemented in C++
- [ ] IPC client implemented in React
- [ ] Parameter sync working bidirectionally
- [ ] Event dispatcher pattern implemented
- [ ] API endpoints documented
- [ ] Error handling & recovery working

### Task 3: Modern UI MVP
- [ ] All 5 main components fully functional
- [ ] Responsive design working
- [ ] Connected to DSP engine via message bridge
- [ ] Real-time parameter updates working
- [ ] Preset save/load working through new UI
- [ ] Audio metering displaying in real-time

### Task 4: Dynamic Signal Chain
- [ ] Add effect UI working
- [ ] Remove effect UI working
- [ ] Drag & drop reordering working
- [ ] Signal flow visualization showing
- [ ] Enable/disable toggle working
- [ ] Audio processing maintains quality

### Task 5: Testing & QA
- [ ] 90%+ test coverage on critical paths
- [ ] All tests passing (frontend + DSP)
- [ ] No console errors/warnings
- [ ] CPU load acceptable (<40%)
- [ ] Message latency <16ms
- [ ] Performance benchmarks recorded

---

## Success Metrics

| Metric | Target | Actual |
|--------|--------|--------|
| Code Coverage | 90%+ | - |
| Test Pass Rate | 100% | - |
| Build Time | <30s | - |
| Message Latency | <16ms | - |
| CPU Overhead | <2% | - |
| UI Responsiveness | 60 FPS | - |
| Bundle Size | <500KB | - |

---

## Output

**Sprint 7 Deliverables:**
- v0.8.0: React frontend foundation + message bridge + MVP UI
- v1.0.0-beta: Full modern UI with preset bar, device panel, audio meter
- v1.0.0: Complete with dynamic signal chain (Release Candidate)

**Documentation:**
- Frontend architecture guide
- Message bridge API documentation
- Component library documentation
- Setup instructions for development

---

## Next Sprint Planning

**Sprint 8** will continue Phase B with:
- Enhanced DSP parameter controls
- Advanced theme customization
- Plugin architecture planning
- Performance optimization

---

**Sprint 7: Platform Migration Begins** 🚀  
**Phase B: Frontend Foundation** 📱  
**Target:** v0.8.0 → v1.0.0  
**Deadline:** June 21, 2026
