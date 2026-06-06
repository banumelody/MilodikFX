# Sprint 7 UI Analysis - Reusable Component Library

## 📊 Complete UI Element Breakdown

### SECTION 1: Top Navigation Bar
```
[Logo + Version] | [PERFORM] [EDIT] [LIBRARY] [SETTINGS] | [CPU 6%] [48 kHz] [128 samples] | [AUDIO RUNNING]
```

**Reusable Components**:
1. **NavTab** - PERFORM/EDIT/LIBRARY/SETTINGS (4x usage)
   - Props: label, active, onClick, icon
   - Styling: Underline on active, cyan color
   
2. **StatBadge** - CPU 6%, 48 kHz, 128 samples (3x usage)
   - Props: label, value, color
   - Styling: Text + value display

3. **StatusIndicator** - AUDIO RUNNING (1x, reusable for other statuses)
   - Props: status, label, color
   - Styling: Dot + text (green for running, red for stopped, amber for warning)

---

### SECTION 2: Top Control Bar
```
[PRESET] [01A Default Clean ▼] [★] | [SAVE] [SAVE AS] [IMPORT] [EXPORT] | [GLOBAL BYPASS] [OFF]
```

**Reusable Components**:
1. **ControlButton** - SAVE, SAVE AS, IMPORT, EXPORT (4x usage)
   - Props: label, onClick, variant (primary/secondary), size
   - Styling: Dark background, light text, hover effects

2. **Dropdown** - Device selector, PRESET list (generic, multi-use)
   - Props: options, value, onChange, label
   - Styling: Dark theme, arrow icon

3. **ToggleSwitch** - Global Bypass OFF (reusable for effect enables)
   - Props: checked, onChange, label
   - Styling: Round toggle, color changes on state

4. **FavoriteButton** - Star icon
   - Props: isFavorite, onChange
   - Styling: Empty/filled star

---

### SECTION 3: Left Sidebar
```
DEVICE:
  INPUT: [Focusrite USB ASIO Input 1 ▼]
  OUTPUT: [Focusrite USB ASIO Output 1 ▼]
  SAMPLE RATE: [48 kHz ▼]
  BUFFER SIZE: [128 samples (2.7 ms) ▼]
  INPUT LEVEL: [-12.4 dB] [Meter Bar]
  OUTPUT LEVEL: [-10.1 dB] [Meter Bar]
  
TUNER:
  [E] [Display] [-50 ... CENT ... +50]
  Needle dial
  E2 82.41 Hz | CENT -1

SCENE:
  [1 Clean] [Grid 4x4]
  
TAP:
  [TAP] [120.0 BPM] [BPM TEMPO]
```

**Reusable Components**:
1. **LabeledValue** - "INPUT LEVEL: -12.4 dB" (8x usage across all panels)
   - Props: label, value, color, units
   - Styling: Label bold, value highlighted

2. **MeterBar** - Horizontal bar with gradient (4x usage: input/output level meters)
   - Props: value (0-1), min, max, color, peakHold
   - Styling: Green gradient baseline → red at peaks, segmented like real meters

3. **Dropdown** - Input/Output/Sample Rate/Buffer (4x usage)
   - Props: label, options, value, onChange
   - Styling: Dark background, chevron icon

4. **TunerDisplay** - Frequency + note + cents
   - Props: frequency, note, cents, isActive
   - Styling: Cyan display, large font

5. **NeedleDial** - Tuner needle (1x, but reusable pattern)
   - Props: value (cents -50 to +50), size
   - Styling: Analog gauge appearance

6. **SceneGrid** - 4x4 button grid (1x, reusable for other grids)
   - Props: items [], selectedId, onClick
   - Styling: Green border if selected, dark fill

7. **TapButton** - Big tap button
   - Props: onClick, bpm, label
   - Styling: Large, prominent, responsive

---

### SECTION 4: Signal Chain (Main Canvas)
```
[IN] → [GATE] → [COMP] → [BOOST] → [OVERDRIVE] → [EQ] → [DELAY] → [REVERB] → [OUT]
(with visual SVG lines connecting)
```

**Reusable Components**:
1. **SignalChainBlock** - Each effect box (GATE, COMP, BOOST, etc.)
   - Props: effectId, name, color, isConnected, isDragging, onDrag, onRemove
   - Styling: Rounded rect, color border, icon inside, color varies by type
   - Makes draggable via react-beautiful-dnd

2. **SignalChainConnector** - SVG line between blocks
   - Props: fromBlock, toBlock (positions)
   - Styling: Cyan line, arrow at end, smooth curve

3. **SignalChainCanvas** - Container with drag-drop logic
   - Props: effects [], onReorder, onRemove, onAdd
   - Internal: Manages layout, renders blocks + connectors

4. **AddBlockButton** - "+ ADD BLOCK" button before [OUT]
   - Props: onClick
   - Styling: Dashed border, plus icon

5. **ClearChainButton** - "🗑 CLEAR CHAIN"
   - Props: onClick
   - Styling: Warning red, trash icon

---

### SECTION 5: Effect Cards Grid
```
[NOISE GATE] [COMPRESSOR] [CLEAN BOOST] [OVERDRIVE] [3 BAND EQ] [DELAY] [REVERB]
  Each card has:
    - Title
    - 2-3 Rows of Knobs
    - 1 Row of Labels (values)
    - ON/OFF Toggle at bottom
```

**Reusable Components**:
1. **EffectCard** - Container for each effect (7x usage)
   - Props: effectId, type, title, color, params [], enabled, onChange
   - Styling: Color-coded border, dark background, compact layout

2. **KnobGroup** - Container for 2-3 knobs in row
   - Props: knobs []
   - Styling: Flex row, spacing

3. **Knob** - Rotatable parameter control (EXISTING, reuse!)
   - Props: value, min, max, onChange, label, color, units
   - Styling: Canvas-based, color matches effect

4. **ParameterLabel** - Below knob showing value
   - Props: label, value, units, color
   - Styling: Small font, centered, mono font for values

5. **EffectToggle** - ON/OFF at bottom of card
   - Props: enabled, onChange, color
   - Styling: Colored dot + ON/OFF text

6. **EffectCardRemoveButton** - X or trash icon
   - Props: onClick
   - Styling: Corner icon, hover shows red

---

### SECTION 6: Scene Buttons (Bottom Left)
```
SCENE
[1] [2] [3] [4]
[Clean] [Crunch] [Lead] [Solo]
[Color Grid] (4x4 visible effect states)
```

**Reusable Components**:
1. **SceneButton** - Numbered scene button (4x usage)
   - Props: sceneNum, name, isActive, onClick, colors []
   - Styling: Border highlight if active

2. **SceneEffectGrid** - 4x4 dot grid showing effect enable states
   - Props: effectStates [] (4 bits per effect x 4 scenes = visual)
   - Styling: Colored dots per effect, white if disabled

---

### SECTION 7: Expression Pedal Assignment
```
EXP / ASSIGN
[EXP 1] [Drive ▼]
[EXP 2] [Delay Mix ▼]
[EXP 3] [Reverb Mix ▼]
```

**Reusable Components**:
1. **ExpressionAssignment** - Single EXP row (3x usage)
   - Props: expNum, selectedParam, availableParams, onChange
   - Styling: Label + dropdown

2. **ExpressionAssignmentGroup** - Container for all 3
   - Props: assignments {exp1, exp2, exp3}, onChange
   - Styling: Flex column, spacing

---

### SECTION 8: Output Section (Right Panel)
```
OUTPUT
Master Volume [Knob] [0 dB] [Meter]
MUTE [Button]

CPU HISTORY [Line Chart 60s]

NOTES [Text Field]
```

**Reusable Components**:
1. **MasterVolumeKnob** - Large knob + meter
   - Props: value, onChange, onMute
   - Styling: Large size (100px), green meter

2. **PerformanceGraph** - CPU history line chart
   - Props: data [] (60 values), max, min
   - Styling: Canvas or recharts, color zones (green/amber/red)

3. **NotesField** - Text area
   - Props: value, onChange, maxLength
   - Styling: Dark background, 500 char limit, placeholder

---

### SECTION 9: Metronome
```
METRONOME
[OFF] [Icon]
[120.0 BPM]
```

**Reusable Components**:
1. **MetronomeControl**
   - Props: enabled, onClick, bpm
   - Styling: Toggle button with icon

---

## 📦 COMPONENT LIBRARY SUMMARY

### 🔧 Atomic Components (Reuse Extensively)
1. **Knob** ✅ (exists, extend)
2. **Button** (variants: primary, secondary, danger) ✅ (exists, extend)
3. **ToggleSwitch** ✅ (simple version exists, enhance)
4. **Dropdown** ✅ (exists, enhance)
5. **Input** ✅ (exists, extend)
6. **Label** ✅ (exists, extend)

### 🎨 Composite Components (New)
1. **LabeledValue** - Reusable across ALL panels
2. **MeterBar** - Input/output level + CPU meter visualization
3. **EffectCard** - Core effect UI unit
4. **SignalChainBlock** - Draggable effect block
5. **SignalChainConnector** - SVG connector line
6. **TunerDisplay** - Frequency + note display
7. **NeedleDial** - Analog gauge
8. **SceneGrid** - 4x4 button matrix
9. **SceneButton** - Individual scene selector
10. **ExpressionAssignment** - EXP mapping UI
11. **MasterVolumeKnob** - Large knob + meter combo
12. **PerformanceGraph** - CPU history chart
13. **NavTab** - Top navigation tabs
14. **StatusIndicator** - Status badge

### 🎭 Container Components (New)
1. **SignalChainCanvas** - Main signal chain area with drag-drop
2. **EffectCardsGrid** - Grid layout for effect cards
3. **LeftPanel** - Device + tuner + scene controls
4. **RightPanel** - Output + CPU + notes
5. **TopBar** - Navigation + presets + stats
6. **Dashboard** - Main layout container

---

## 🎯 Optimization Strategies

### 1. Props Patterns (Reusability)
```typescript
// All labeled values use same pattern
interface LabeledProps {
  label: string;
  value: string | number;
  units?: string;
  color?: 'primary' | 'secondary' | 'success' | 'warning' | 'error';
}

// All toggles use same pattern
interface TogglableProps {
  enabled: boolean;
  onChange: (enabled: boolean) => void;
  color?: string;
}

// All draggable components
interface DraggableProps {
  id: string;
  isDragging?: boolean;
  onDrag?: () => void;
}
```

### 2. Style Composition
- Use TailwindCSS utility classes everywhere
- Define color maps for effect types: `effectTypeColors = { GATE: 'green', COMP: 'blue', ...}`
- All knobs inherit from base Knob component
- All buttons inherit from base Button component

### 3. State Management
- useAudioEngine hook for all audio parameters
- useTheme hook for dark/light
- useSignalChain hook for effect chain state
- useScenes hook for scene management

### 4. Performance Optimizations
- Memoize expensive renders: Effect cards, meters, graph
- Virtual scrolling for effect list (if >10 effects)
- Canvas rendering for Knobs + Meters + Tuner
- Throttle real-time updates (metering at 10Hz, not callback rate)

---

## 🚀 Implementation Roadmap

### Phase 1: Core Atomics (Extend Existing)
- [ ] Enhance Knob component
- [ ] Enhance Button with variants
- [ ] Create Dropdown component
- [ ] Create ToggleSwitch component
- [ ] Create Input component

### Phase 2: Build Composite Components
- [ ] LabeledValue
- [ ] MeterBar
- [ ] EffectCard
- [ ] TunerDisplay
- [ ] SignalChainBlock

### Phase 3: Signal Chain & Drag-Drop
- [ ] SignalChainCanvas (with react-beautiful-dnd)
- [ ] SignalChainConnector (SVG)
- [ ] AddBlockButton / ClearChainButton

### Phase 4: Panels & Containers
- [ ] LeftPanel (device + tuner + scene)
- [ ] RightPanel (output + CPU + notes)
- [ ] TopBar (nav + presets)
- [ ] EffectCardsGrid

### Phase 5: Advanced Features
- [ ] ExpressionAssignment
- [ ] PerformanceGraph (recharts)
- [ ] SceneGrid
- [ ] NeedleDial tuner

### Phase 6: Main Dashboard
- [ ] Integrate all components
- [ ] Responsive layout
- [ ] Dark/light theme
- [ ] IPC integration

---

## 📈 Code Organization

```
frontend/src/
├── components/
│   ├── atomic/                 (Base components - reusable)
│   │   ├── Knob.tsx           (existing - enhance)
│   │   ├── Button.tsx          (existing - enhance)
│   │   ├── ToggleSwitch.tsx    (existing - enhance)
│   │   ├── Dropdown.tsx        (enhance)
│   │   ├── Input.tsx           (existing)
│   │   └── Label.tsx           (existing)
│   │
│   ├── composite/              (Built from atomics - reusable blocks)
│   │   ├── LabeledValue.tsx
│   │   ├── MeterBar.tsx
│   │   ├── EffectCard.tsx
│   │   ├── TunerDisplay.tsx
│   │   ├── NeedleDial.tsx
│   │   ├── SignalChainBlock.tsx
│   │   ├── SignalChainConnector.tsx
│   │   ├── SceneButton.tsx
│   │   ├── SceneGrid.tsx
│   │   ├── ExpressionAssignment.tsx
│   │   ├── MasterVolumeKnob.tsx
│   │   ├── PerformanceGraph.tsx
│   │   ├── NavTab.tsx
│   │   └── StatusIndicator.tsx
│   │
│   ├── containers/             (Layout + logic)
│   │   ├── SignalChainCanvas.tsx
│   │   ├── EffectCardsGrid.tsx
│   │   ├── LeftPanel.tsx
│   │   ├── RightPanel.tsx
│   │   ├── TopBar.tsx
│   │   ├── MainLayout.tsx
│   │   └── Dashboard.tsx
│   │
│   └── pages/
│       ├── PerformTab.tsx      (Perform view - main)
│       ├── EditTab.tsx         (Effect parameter editor)
│       ├── LibraryTab.tsx      (Preset browser)
│       └── SettingsTab.tsx     (App settings)
│
├── hooks/
│   ├── useAudioEngine.ts       (existing)
│   ├── useSignalChain.ts       (NEW - effect chain state)
│   ├── useScenes.ts            (NEW - scene management)
│   ├── usePerformanceMetrics.ts (NEW - CPU/latency tracking)
│   ├── useTheme.ts             (existing)
│   └── useMIDIAssignment.ts    (NEW - EXP mapping)
│
├── services/
│   ├── messageBridge.ts        (IPC client - existing)
│   ├── signalChainService.ts   (NEW - effect chain API)
│   ├── sceneService.ts         (NEW - scene save/load)
│   ├── tunerService.ts         (NEW - frequency detection)
│   └── metronomService.ts      (NEW - tempo/tap)
│
├── utils/
│   ├── effectTypeColors.ts     (NEW - color map)
│   ├── audioUtils.ts           (NEW - dB conversions, etc)
│   └── validators.ts           (NEW - input validation)
│
└── __tests__/
    ├── components/
    │   ├── LabeledValue.test.tsx
    │   ├── MeterBar.test.tsx
    │   ├── EffectCard.test.tsx
    │   ├── SignalChainCanvas.test.tsx
    │   └── ... (comprehensive coverage)
    │
    ├── hooks/
    │   ├── useSignalChain.test.ts
    │   ├── useScenes.test.ts
    │   └── ...
    │
    └── integration/
        ├── signalChainWorkflow.test.tsx
        ├── sceneWorkflow.test.tsx
        └── ...
```

---

## ✅ Reusability Metrics

**Goal**: Minimize duplicated code, maximize component reuse

| Component | Usage Count | Lines of Code | Reusability |
|-----------|-------------|---------------|------------|
| Knob | 12+ (effect params) | ~150 | ⭐⭐⭐⭐⭐ |
| Button | 10+ (actions) | ~100 | ⭐⭐⭐⭐⭐ |
| ToggleSwitch | 8+ (effect enables) | ~80 | ⭐⭐⭐⭐⭐ |
| Dropdown | 8+ (selectors) | ~120 | ⭐⭐⭐⭐⭐ |
| LabeledValue | 15+ (display values) | ~60 | ⭐⭐⭐⭐⭐ |
| MeterBar | 4+ (audio levels) | ~100 | ⭐⭐⭐⭐ |
| EffectCard | 8+ (per effect) | ~200 | ⭐⭐⭐⭐ |
| SignalChainBlock | 8+ (effects in chain) | ~120 | ⭐⭐⭐⭐ |

---

## 🎯 Expected Outcomes

- **Code Duplication**: < 5%
- **Component Reuse**: 70%+ of UI built from 20 base components
- **Bundle Size**: Stay within 500KB gzipped
- **Development Speed**: 30% faster due to component library
- **Maintenance**: 40% easier due to single source of truth

