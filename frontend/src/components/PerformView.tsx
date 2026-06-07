import React, { useState, useEffect, useRef } from 'react';
import './PerformView.css';

// ─── Types ──────────────────────────────────────────────────────────────
interface EffectParam {
  name: string;
  value: number;
  displayValue: string;
  min: number;
  max: number;
}

interface EffectBlock {
  id: string;
  name: string;
  shortName: string;
  type: string;
  color: string;
  enabled: boolean;
  icon: string;
  params: EffectParam[];
}

interface Scene {
  id: number;
  name: string;
  effectStates: boolean[]; // on/off per effect in chain
}

interface ExpAssignment {
  expNum: number;
  target: string;
}

// ─── SVG Knob Component ────────────────────────────────────────────────
const SvgKnob: React.FC<{
  value: number;
  min: number;
  max: number;
  displayValue: string;
  label: string;
  size?: number;
  color?: string;
  onChange?: (v: number) => void;
  isSync?: boolean;
  syncLabel?: string;
}> = ({ value, min, max, displayValue, label, size = 56, color = '#6b7280', onChange, isSync, syncLabel }) => {
  const normalized = (value - min) / (max - min);
  const startAngle = 225;
  const endAngle = -45;
  const range = startAngle - endAngle;
  const angle = startAngle - normalized * range;
  const r = size / 2 - 6;
  const cx = size / 2;
  const cy = size / 2;

  // Arc path
  const toRad = (deg: number) => (deg * Math.PI) / 180;
  const arcStart = { x: cx + r * Math.cos(toRad(-startAngle + 90)), y: cy - r * Math.sin(toRad(-startAngle + 90)) };
  const arcEnd = { x: cx + r * Math.cos(toRad(-angle + 90)), y: cy - r * Math.sin(toRad(-angle + 90)) };
  const largeArc = normalized * range > 180 ? 1 : 0;

  // Indicator line
  const indicatorR = r * 0.6;
  const ix = cx + indicatorR * Math.cos(toRad(-angle + 90));
  const iy = cy - indicatorR * Math.sin(toRad(-angle + 90));

  // Drag state
  const knobRef = useRef<SVGSVGElement>(null);
  const dragging = useRef(false);
  const lastY = useRef(0);

  const handleMouseDown = (e: React.MouseEvent) => {
    dragging.current = true;
    lastY.current = e.clientY;
    e.preventDefault();
  };

  useEffect(() => {
    const handleMove = (e: MouseEvent) => {
      if (!dragging.current || !onChange) return;
      const dy = lastY.current - e.clientY;
      const sensitivity = (max - min) / 150;
      const newVal = Math.min(max, Math.max(min, value + dy * sensitivity));
      onChange(newVal);
      lastY.current = e.clientY;
    };
    const handleUp = () => { dragging.current = false; };
    window.addEventListener('mousemove', handleMove);
    window.addEventListener('mouseup', handleUp);
    return () => { window.removeEventListener('mousemove', handleMove); window.removeEventListener('mouseup', handleUp); };
  }, [value, min, max, onChange]);

  if (isSync) {
    return (
      <div className="perform-knob-container">
        <div className="perform-sync-box" style={{ width: size, height: size, borderColor: color }}>
          <span className="perform-sync-label">{syncLabel || displayValue}</span>
        </div>
        <span className="perform-knob-label">{label}</span>
      </div>
    );
  }

  return (
    <div className="perform-knob-container">
      <svg
        ref={knobRef}
        width={size}
        height={size}
        onMouseDown={handleMouseDown}
        style={{ cursor: 'grab' }}
        className="perform-knob-svg"
      >
        {/* Track */}
        <circle cx={cx} cy={cy} r={r} fill="none" stroke="#2a2e35" strokeWidth="4" />
        {/* Value arc */}
        {normalized > 0.001 && (
          <path
            d={`M ${arcStart.x} ${arcStart.y} A ${r} ${r} 0 ${largeArc} 1 ${arcEnd.x} ${arcEnd.y}`}
            fill="none"
            stroke={color}
            strokeWidth="4"
            strokeLinecap="round"
          />
        )}
        {/* Inner circle */}
        <circle cx={cx} cy={cy} r={r * 0.65} fill="#1a1d23" />
        {/* Indicator dot */}
        <circle cx={ix} cy={iy} r={3} fill="#e0e0e0" />
      </svg>
      <span className="perform-knob-value">{displayValue}</span>
      <span className="perform-knob-label">{label}</span>
    </div>
  );
};

// ─── Vertical Meter ────────────────────────────────────────────────────
const VerticalMeter: React.FC<{ value: number; color?: string; height?: number }> = ({
  value,
  color = '#22c55e',
  height = 200,
}) => {
  const dbMin = -60;
  const dbMax = 0;
  const pct = Math.max(0, Math.min(1, (value - dbMin) / (dbMax - dbMin)));
  return (
    <div className="perform-vmeter" style={{ height }}>
      <div className="perform-vmeter-track">
        <div
          className="perform-vmeter-fill"
          style={{
            height: `${pct * 100}%`,
            background: `linear-gradient(to top, ${color}, ${pct > 0.8 ? '#ef4444' : color})`,
          }}
        />
      </div>
      {/* Scale marks */}
      {[0, -6, -12, -18, -24, -36, -60].map(db => (
        <div
          key={db}
          className="perform-vmeter-mark"
          style={{ bottom: `${((db - dbMin) / (dbMax - dbMin)) * 100}%` }}
        >
          <span>{db}</span>
        </div>
      ))}
    </div>
  );
};

// ─── Horizontal Meter Bar ──────────────────────────────────────────────
const HorizontalMeterBar: React.FC<{ value: number; label: string; displayDb: string }> = ({ value, label, displayDb }) => {
  const dbMin = -60;
  const dbMax = 0;
  const pct = Math.max(0, Math.min(1, (value - dbMin) / (dbMax - dbMin)));

  return (
    <div className="perform-hmeter">
      <div className="perform-hmeter-header">
        <span className="perform-hmeter-label">{label}</span>
        <span className="perform-hmeter-db">{displayDb}</span>
      </div>
      <div className="perform-hmeter-track">
        <div
          className="perform-hmeter-fill"
          style={{ width: `${pct * 100}%` }}
        />
      </div>
      <div className="perform-hmeter-scale">
        <span>-60</span><span>-36</span><span>-18</span><span>-6</span><span>0</span>
      </div>
    </div>
  );
};

// ─── Main PerformView Component ────────────────────────────────────────
export const PerformView: React.FC = () => {
  // ── State ─────────────────────────────────────────────────────────────
  const [activeTab, setActiveTab] = useState<'perform' | 'edit' | 'library' | 'settings'>('perform');
  const [globalBypass, setGlobalBypass] = useState(false);
  const [presetName] = useState('01A  Default Clean');
  const [masterVolume, setMasterVolume] = useState(0);
  const [isMuted, setIsMuted] = useState(false);

  // Device
  const [inputDevice] = useState('Focusrite USB ASIO Input 1');
  const [outputDevice] = useState('Focusrite USB ASIO Output 1');
  const [sampleRate] = useState('48 kHz');
  const [bufferSize] = useState('128 samples (2.7 ms)');

  // Meters
  const [inputLevel, setInputLevel] = useState(-12.4);
  const [outputLevel, setOutputLevel] = useState(-10.1);
  const [outputMeterLevel, setOutputMeterLevel] = useState(-10);

  // Tuner
  const [tunerNote] = useState('E');
  const [tunerFreq] = useState(82.41);
  const [tunerCent] = useState(-1);

  // Tap / Metronome
  const [bpm] = useState(120.0);
  const [metronomeOn] = useState(false);
  const [notes, setNotes] = useState('');

  // CPU
  const [cpuLoad, setCpuLoad] = useState(6);
  const [cpuHistory, setCpuHistory] = useState<number[]>(Array(30).fill(6));

  // Effects chain
  const [effects, setEffects] = useState<EffectBlock[]>([
    {
      id: 'gate', name: 'NOISE GATE', shortName: 'GATE', type: 'gate', color: '#22c55e',
      enabled: true, icon: '⊓',
      params: [
        { name: 'Threshold', value: -60, displayValue: '-60.0 dB', min: -80, max: 0 },
        { name: 'Release', value: 120, displayValue: '120 ms', min: 1, max: 500 },
      ],
    },
    {
      id: 'comp', name: 'COMPRESSOR', shortName: 'COMP', type: 'compressor', color: '#3b82f6',
      enabled: true, icon: 'o-o-o',
      params: [
        { name: 'Threshold', value: -24, displayValue: '-24.0 dB', min: -60, max: 0 },
        { name: 'Ratio', value: 4, displayValue: '4.0:1', min: 1, max: 20 },
        { name: 'Attack', value: 10, displayValue: '10 ms', min: 0.1, max: 100 },
        { name: 'Release', value: 80, displayValue: '80 ms', min: 1, max: 500 },
      ],
    },
    {
      id: 'boost', name: 'CLEAN BOOST', shortName: 'BOOST', type: 'boost', color: '#eab308',
      enabled: true, icon: '⊿',
      params: [
        { name: 'Gain', value: 6, displayValue: '6.0 dB', min: 0, max: 24 },
        { name: 'Level', value: 0, displayValue: '0.0 dB', min: -12, max: 12 },
      ],
    },
    {
      id: 'od', name: 'OVERDRIVE', shortName: 'OVERDRIVE', type: 'overdrive', color: '#f97316',
      enabled: true, icon: '∿',
      params: [
        { name: 'Drive', value: 35, displayValue: '35 %', min: 0, max: 100 },
        { name: 'Tone', value: 50, displayValue: '50 %', min: 0, max: 100 },
        { name: 'Level', value: 70, displayValue: '70 %', min: 0, max: 100 },
      ],
    },
    {
      id: 'eq', name: '3 BAND EQ', shortName: 'EQ', type: 'eq', color: '#14b8a6',
      enabled: true, icon: '≋',
      params: [
        { name: 'Bass', value: -2, displayValue: '-2.0 dB', min: -12, max: 12 },
        { name: 'Mid', value: 3, displayValue: '+3.0 dB', min: -12, max: 12 },
      ],
    },
    {
      id: 'delay', name: 'DELAY', shortName: 'DELAY', type: 'delay', color: '#a855f7',
      enabled: true, icon: '⊞',
      params: [
        { name: 'Time', value: 400, displayValue: '400 ms', min: 1, max: 2000 },
        { name: 'Feedback', value: 35, displayValue: '35 %', min: 0, max: 100 },
        { name: 'Mix', value: 25, displayValue: '25 %', min: 0, max: 100 },
        { name: 'Sync', value: 0, displayValue: '1/4', min: 0, max: 1 },
      ],
    },
    {
      id: 'reverb', name: 'REVERB', shortName: 'REVERB', type: 'reverb', color: '#06b6d4',
      enabled: true, icon: ')))' ,
      params: [
        { name: 'Decay', value: 2.8, displayValue: '2.8 s', min: 0.1, max: 10 },
        { name: 'Pre Delay', value: 20, displayValue: '20 ms', min: 0, max: 200 },
      ],
    },
  ]);

  // Scenes
  const [activeScene, setActiveScene] = useState(1);
  const [scenes] = useState<Scene[]>([
    { id: 1, name: 'Clean', effectStates: [true, true, true, false, true, false, false] },
    { id: 2, name: 'Crunch', effectStates: [true, true, false, true, true, false, false] },
    { id: 3, name: 'Lead', effectStates: [true, true, false, true, true, true, true] },
    { id: 4, name: 'Solo', effectStates: [true, false, false, true, true, true, true] },
  ]);

  // Expression assignments
  const [expAssignments] = useState<ExpAssignment[]>([
    { expNum: 1, target: 'Drive' },
    { expNum: 2, target: 'Delay Mix' },
    { expNum: 3, target: 'Reverb Mix' },
  ]);

  // ── Simulated audio updates ────────────────────────────────────────
  useEffect(() => {
    const interval = setInterval(() => {
      setInputLevel(-15 + Math.random() * 6);
      setOutputLevel(-13 + Math.random() * 6);
      setOutputMeterLevel(-12 + Math.random() * 5);
      setCpuLoad(prev => {
        const next = Math.max(1, Math.min(15, prev + (Math.random() - 0.5) * 2));
        setCpuHistory(h => [...h.slice(1), next]);
        return next;
      });
    }, 250);
    return () => clearInterval(interval);
  }, []);

  // ── Toggle effect ─────────────────────────────────────────────────
  const toggleEffect = (id: string) => {
    setEffects(prev => prev.map(e => e.id === id ? { ...e, enabled: !e.enabled } : e));
  };

  // ── CPU History Canvas ────────────────────────────────────────────
  const cpuCanvasRef = useRef<HTMLCanvasElement>(null);
  useEffect(() => {
    const canvas = cpuCanvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    const w = canvas.width;
    const h = canvas.height;
    ctx.clearRect(0, 0, w, h);

    // Grid lines
    ctx.strokeStyle = '#1f2937';
    ctx.lineWidth = 1;
    for (let i = 0; i <= 4; i++) {
      const y = (i / 4) * h;
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(w, y);
      ctx.stroke();
    }

    // CPU line
    ctx.strokeStyle = '#22c55e';
    ctx.lineWidth = 2;
    ctx.beginPath();
    cpuHistory.forEach((val, i) => {
      const x = (i / (cpuHistory.length - 1)) * w;
      const y = h - (val / 100) * h;
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    });
    ctx.stroke();
  }, [cpuHistory]);

  // ── Render ────────────────────────────────────────────────────────
  return (
    <div className="perform-root">
      {/* ═══════════════ TOP BAR ═══════════════ */}
      <header className="perform-topbar">
        {/* Logo */}
        <div className="perform-logo">
          <span className="perform-logo-icon">𝄞</span>
          <div>
            <div className="perform-logo-title">MilodikFX</div>
            <div className="perform-logo-version">v0.6.0</div>
          </div>
        </div>

        {/* Nav Tabs */}
        <nav className="perform-nav">
          {(['perform', 'edit', 'library', 'settings'] as const).map(tab => (
            <button
              key={tab}
              className={`perform-nav-tab ${activeTab === tab ? 'active' : ''}`}
              onClick={() => setActiveTab(tab)}
            >
              {tab.toUpperCase()}
            </button>
          ))}
        </nav>

        {/* CPU / Status */}
        <div className="perform-topbar-status">
          <div className="perform-cpu-mini">
            <span className="perform-cpu-label">CPU</span>
            <span className="perform-cpu-value" style={{ color: cpuLoad > 50 ? '#ef4444' : '#22c55e' }}>
              {cpuLoad.toFixed(0)}%
            </span>
            <span className="perform-cpu-indicator" style={{ background: cpuLoad > 50 ? '#ef4444' : '#22c55e' }} />
          </div>
          <div className="perform-audio-info">
            <span>{sampleRate}</span>
            <span>{bufferSize.split('(')[0].trim()}</span>
          </div>
          <div className="perform-audio-running">
            <span className="perform-running-dot" />
            <span>AUDIO RUNNING</span>
          </div>
        </div>
      </header>

      {/* ═══════════════ PRESET BAR ═══════════════ */}
      <div className="perform-preset-bar">
        <div className="perform-preset-left">
          <span className="perform-preset-label">PRESET</span>
          <div className="perform-preset-selector">
            <span className="perform-preset-name">{presetName}</span>
            <span className="perform-preset-chevron">▾</span>
          </div>
          <button className="perform-preset-star">☆</button>
        </div>
        <div className="perform-preset-actions">
          <button className="perform-btn-sm">SAVE</button>
          <button className="perform-btn-sm">SAVE AS</button>
          <button className="perform-btn-sm">IMPORT</button>
          <button className="perform-btn-sm">EXPORT</button>
        </div>
        <div className="perform-bypass">
          <span>GLOBAL BYPASS</span>
          <button
            className={`perform-bypass-toggle ${globalBypass ? 'on' : 'off'}`}
            onClick={() => setGlobalBypass(!globalBypass)}
          >
            <span className="perform-bypass-knob" />
            <span className="perform-bypass-label">{globalBypass ? 'ON' : 'OFF'}</span>
          </button>
        </div>
      </div>

      {/* ═══════════════ MAIN CONTENT ═══════════════ */}
      <div className="perform-main">
        {/* ─── LEFT SIDEBAR ─── */}
        <aside className="perform-sidebar-left">
          {/* Device Section */}
          <div className="perform-section">
            <div className="perform-section-header">
              <span>DEVICE</span>
              <button className="perform-collapse-btn">⌃</button>
            </div>
            <div className="perform-field">
              <label>INPUT</label>
              <select className="perform-select" defaultValue={inputDevice}>
                <option>{inputDevice}</option>
              </select>
            </div>
            <div className="perform-field">
              <label>OUTPUT</label>
              <select className="perform-select" defaultValue={outputDevice}>
                <option>{outputDevice}</option>
              </select>
            </div>
            <div className="perform-field">
              <label>SAMPLE RATE</label>
              <select className="perform-select" defaultValue={sampleRate}>
                <option>{sampleRate}</option>
              </select>
            </div>
            <div className="perform-field">
              <label>BUFFER SIZE</label>
              <select className="perform-select" defaultValue={bufferSize}>
                <option>{bufferSize}</option>
              </select>
            </div>
          </div>

          {/* Input / Output Level */}
          <div className="perform-section">
            <HorizontalMeterBar value={inputLevel} label="INPUT LEVEL" displayDb={`${inputLevel.toFixed(1)} dB`} />
            <HorizontalMeterBar value={outputLevel} label="OUTPUT LEVEL" displayDb={`${outputLevel.toFixed(1)} dB`} />
          </div>

          {/* Tuner */}
          <div className="perform-section">
            <div className="perform-section-header"><span>TUNER</span></div>
            <div className="perform-tuner">
              <div className="perform-tuner-note">{tunerNote}</div>
              <div className="perform-tuner-needle">
                <div className="perform-tuner-scale">
                  <span>-50</span>
                  <span>+50</span>
                </div>
                <div className="perform-tuner-bar">
                  <div
                    className="perform-tuner-indicator"
                    style={{ left: `${50 + tunerCent}%` }}
                  />
                </div>
              </div>
              <div className="perform-tuner-info">
                <span>{tunerNote}2</span>
                <span>{tunerFreq.toFixed(2)} Hz</span>
                <span>CENT</span>
                <span>{tunerCent}</span>
              </div>
            </div>
          </div>
        </aside>

        {/* ─── CENTER CONTENT ─── */}
        <main className="perform-center">
          {/* Signal Chain */}
          <div className="perform-signal-chain-section">
            <div className="perform-signal-chain-header">
              <span className="perform-section-title">SIGNAL CHAIN</span>
              <div className="perform-signal-chain-actions">
                <button className="perform-btn-sm">+ ADD BLOCK</button>
                <button className="perform-btn-sm perform-btn-danger">🗑 CLEAR CHAIN</button>
              </div>
            </div>

            {/* Visual Signal Chain */}
            <div className="perform-chain-visual">
              {/* IN */}
              <div className="perform-chain-endpoint">
                <div className="perform-chain-icon">🎸</div>
                <span>IN</span>
              </div>

              {effects.map((effect) => (
                <React.Fragment key={effect.id}>
                  <div className="perform-chain-connector" />
                  <div
                    className={`perform-chain-block ${effect.enabled ? '' : 'disabled'}`}
                    style={{ borderColor: effect.color }}
                    onClick={() => toggleEffect(effect.id)}
                  >
                    <div className="perform-chain-block-icon" style={{ color: effect.color }}>{effect.icon}</div>
                    <span className="perform-chain-block-name">{effect.shortName}</span>
                  </div>
                </React.Fragment>
              ))}

              <div className="perform-chain-connector" />
              {/* OUT */}
              <div className="perform-chain-endpoint">
                <div className="perform-chain-icon">🔊</div>
                <span>OUT</span>
              </div>
            </div>
          </div>

          {/* Effect Cards */}
          <div className="perform-effect-cards">
            {effects.map(effect => (
              <div
                key={effect.id}
                className="perform-effect-card"
                style={{ borderTopColor: effect.color }}
              >
                <div className="perform-effect-card-header" style={{ color: effect.color }}>
                  <span className="perform-effect-card-name">{effect.name}</span>
                  <button className="perform-effect-power">⏻</button>
                </div>
                <div className="perform-effect-card-params">
                  {effect.params.map(param => (
                    <SvgKnob
                      key={param.name}
                      value={param.value}
                      min={param.min}
                      max={param.max}
                      displayValue={param.displayValue}
                      label={param.name}
                      size={52}
                      color={effect.color}
                      isSync={param.name === 'Sync'}
                      syncLabel={param.displayValue}
                    />
                  ))}
                </div>
                <div className="perform-effect-card-footer">
                  <span
                    className="perform-effect-status-dot"
                    style={{ background: effect.enabled ? effect.color : '#4b5563' }}
                  />
                  <span className={`perform-effect-status-text ${effect.enabled ? 'on' : 'off'}`}>
                    ON
                  </span>
                </div>
              </div>
            ))}
          </div>

          {/* Bottom Row: Scene / Expression / Output */}
          <div className="perform-bottom-row">
            {/* Scene Grid */}
            <div className="perform-scene-panel">
              <div className="perform-scene-header">
                <span className="perform-section-title">SCENE</span>
                <div className="perform-scene-tabs">
                  {[1, 2, 3, 4].map(n => (
                    <button
                      key={n}
                      className={`perform-scene-tab ${activeScene === n ? 'active' : ''}`}
                      onClick={() => setActiveScene(n)}
                    >
                      {n}
                    </button>
                  ))}
                </div>
              </div>
              <div className="perform-scene-grid">
                {scenes.map(scene => (
                  <div key={scene.id} className="perform-scene-row">
                    <span className="perform-scene-num">{scene.id}</span>
                    <span className="perform-scene-name">{scene.name}</span>
                    {scene.effectStates.map((on, idx) => (
                      <span
                        key={idx}
                        className={`perform-scene-cell ${on ? 'on' : 'off'}`}
                        style={on ? { background: effects[idx]?.color || '#555' } : {}}
                      />
                    ))}
                  </div>
                ))}
              </div>
            </div>

            {/* Expression / Assign */}
            <div className="perform-exp-panel">
              <span className="perform-section-title">EXP / ASSIGN</span>
              <div className="perform-exp-content">
                <div className="perform-exp-pedal">
                  <div className="perform-exp-pedal-visual">
                    <div className="perform-exp-label">EXP 1</div>
                    <div className="perform-exp-pedal-box" />
                  </div>
                </div>
                <div className="perform-exp-assigns">
                  <span className="perform-exp-assign-header">ASSIGN</span>
                  {expAssignments.map(exp => (
                    <div key={exp.expNum} className="perform-exp-row">
                      <span className="perform-exp-num">EXP {exp.expNum}</span>
                      <select className="perform-select perform-select-sm">
                        <option>{exp.target}</option>
                      </select>
                    </div>
                  ))}
                </div>
              </div>
            </div>

            {/* Output Section */}
            <div className="perform-output-panel">
              <span className="perform-section-title">OUTPUT</span>
              <div className="perform-output-content">
                <div className="perform-output-knob-area">
                  <span className="perform-output-label">Master Volume</span>
                  <SvgKnob
                    value={masterVolume}
                    min={-60}
                    max={6}
                    displayValue={`${masterVolume.toFixed(1)} dB`}
                    label=""
                    size={80}
                    color="#06b6d4"
                    onChange={setMasterVolume}
                  />
                  <button
                    className={`perform-mute-btn ${isMuted ? 'muted' : ''}`}
                    onClick={() => setIsMuted(!isMuted)}
                  >
                    MUTE
                  </button>
                </div>
                <VerticalMeter value={outputMeterLevel} color="#22c55e" height={160} />
              </div>
            </div>
          </div>
        </main>
      </div>

      {/* ═══════════════ STATUS BAR (BOTTOM) ═══════════════ */}
      <footer className="perform-statusbar">
        <div className="perform-statusbar-tap">
          <button className="perform-tap-btn">TAP</button>
          <div className="perform-tap-info">
            <span className="perform-tap-bpm">{bpm.toFixed(1)} BPM</span>
            <span className="perform-tap-label">TAP TEMPO</span>
          </div>
        </div>

        <div className="perform-statusbar-metronome">
          <span className="perform-metronome-icon">🎵</span>
          <div>
            <span className="perform-metronome-title">METRONOME</span>
            <span className="perform-metronome-state">{metronomeOn ? 'ON' : 'OFF'}</span>
          </div>
        </div>

        <div className="perform-statusbar-notes">
          <span className="perform-notes-label">NOTES</span>
          <input
            className="perform-notes-input"
            placeholder="Write your note here..."
            value={notes}
            onChange={(e) => setNotes(e.target.value)}
          />
        </div>

        <div className="perform-statusbar-cpu">
          <span className="perform-cpu-history-label">CPU HISTORY</span>
          <canvas ref={cpuCanvasRef} width={200} height={40} className="perform-cpu-canvas" />
          <span className="perform-cpu-pct">{cpuLoad.toFixed(0)}%</span>
        </div>
      </footer>
    </div>
  );
};

export default PerformView;
