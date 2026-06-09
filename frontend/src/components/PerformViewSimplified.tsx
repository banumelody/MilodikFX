import { useState, useEffect, useRef } from 'react';
import RotaryKnob from './RotaryKnob';

type EffectItem = { id: string; type: string };

function id(prefix = 'id') {
  return `${prefix}_${Date.now().toString(36)}_${Math.random().toString(36).slice(2,8)}`;
}

export function PerformViewSimplified() {
  const [masterVolume, setMasterVolume] = useState(-6);
  const [inputLevel, setInputLevel] = useState(-12);
  const [outputLevel, setOutputLevel] = useState(-10);

  // UI state for tests / simplified interactions
  const [activeTab, setActiveTab] = useState<'perform' | 'edit' | 'library' | 'settings'>('perform');
  const [showAddEffectModal, setShowAddEffectModal] = useState(false);
  const [currentScene, setCurrentScene] = useState(1);
  const [scenes, setScenes] = useState<Record<number, EffectItem[]>>({ 1: [], 2: [], 3: [] });
  const draggingId = useRef<string | null>(null);
  const [themeDark, setThemeDark] = useState(true);

  // Expose a global setter for E2E testing; keep for test automation
  useEffect(() => {
    (window as any).__setMasterVolume = (v: number) => setMasterVolume(v);
    return () => { delete (window as any).__setMasterVolume; };
  }, []);

  // Ensure initial body class expected by tests
  useEffect(() => {
    if (themeDark) {
      document.body.classList.add('bg-gray-950');
      document.body.classList.remove('bg-white');
    } else {
      document.body.classList.remove('bg-gray-950');
      document.body.classList.add('bg-white');
    }
  }, [themeDark]);

  // Add effect to current scene
  const addEffect = (type: string) => {
    const e: EffectItem = { id: id('e'), type };
    setScenes(prev => ({ ...prev, [currentScene]: [...(prev[currentScene] || []), e] }));
    setShowAddEffectModal(false);
  };

  const removeEffect = (effectId: string) => {
    setScenes(prev => ({ ...prev, [currentScene]: (prev[currentScene] || []).filter(x => x.id !== effectId) }));
  };

  const onDragStart = (e: React.DragEvent, effectId: string) => {
    draggingId.current = effectId;
    e.dataTransfer?.setData('text/plain', effectId);
    e.dataTransfer!.effectAllowed = 'move';
  };

  const onDragOver = (e: React.DragEvent) => {
    e.preventDefault();
    e.dataTransfer!.dropEffect = 'move';
  };

  const onDrop = (e: React.DragEvent, targetId: string) => {
    e.preventDefault();
    const srcId = draggingId.current || e.dataTransfer.getData('text/plain');
    if (!srcId || srcId === targetId) return;
    setScenes(prev => {
      const arr = [...(prev[currentScene] || [])];
      const srcIdx = arr.findIndex(i => i.id === srcId);
      const tgtIdx = arr.findIndex(i => i.id === targetId);
      if (srcIdx === -1 || tgtIdx === -1) return prev;
      const [item] = arr.splice(srcIdx, 1);
      arr.splice(tgtIdx, 0, item);
      return { ...prev, [currentScene]: arr };
    });
    draggingId.current = null;
  };

  // Scene switch preserves chain
  const switchScene = (n: number) => setCurrentScene(n);

  return (
    <div data-testid="app-root" style={{ width: '100vw', height: '100vh', backgroundColor: '#0f172a', color: '#e2e8f0', fontFamily: 'Monaco, monospace', fontSize: '13px', display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>

      {/* HEADER */}
      <header style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '12px 24px', backgroundColor: '#1e293b', borderBottom: '1px solid #334155' }}>
        <div style={{ display: 'flex', gap: '16px', alignItems: 'center' }}>
          <h1 style={{ margin: 0, fontSize: '20px' }}>MilodikFX</h1>
          <span style={{ color: '#64748b' }}>v0.6.0</span>
        </div>

        <nav data-cy="nav-tabs" style={{ display: 'flex', gap: '12px', alignItems: 'center' }}>
          <button data-cy="tab-perform" className={activeTab === 'perform' ? 'active' : ''} onClick={() => setActiveTab('perform')}>PERFORM</button>
          <button data-cy="tab-edit" className={activeTab === 'edit' ? 'active' : ''} onClick={() => setActiveTab('edit')}>EDIT</button>
          <button data-cy="tab-library" className={activeTab === 'library' ? 'active' : ''} onClick={() => setActiveTab('library')}>LIBRARY</button>
          <button data-cy="tab-settings" className={activeTab === 'settings' ? 'active' : ''} onClick={() => setActiveTab('settings')}>SETTINGS</button>
        </nav>

        <div style={{ display: 'flex', gap: 12, alignItems: 'center' }}>
          <button data-cy="theme-toggle" onClick={() => setThemeDark(d => !d)} style={{ padding: '6px 8px' }}>{themeDark ? '☀️' : '🌙'}</button>
          <div style={{ color: '#22c55e', fontSize: '12px' }}>● AUDIO RUNNING</div>
        </div>
      </header>

      {/* MAIN */}
      <main style={{ display: 'flex', flex: 1, gap: 0, overflow: 'hidden' }}>

        {/* LEFT */}
        <aside data-cy="left-panel" style={{ width: 250, backgroundColor: '#1a202c', borderRight: '1px solid #334155', padding: 16, overflowY: 'auto' }}>
          <div style={{ marginBottom: 16 }}>
            <label style={{ display: 'block', marginBottom: 8, color: '#94a3b8', fontSize: 12 }}>PRESET</label>
            <select style={{ width: '100%', padding: 8, backgroundColor: '#334155', color: '#e2e8f0', border: '1px solid #475569', borderRadius: 4 }}>
              <option>01A Default Clean</option>
            </select>
          </div>

          <div style={{ marginBottom: 16 }}>
            <label style={{ display: 'block', marginBottom: 8, color: '#94a3b8', fontSize: 12 }}>Audio Input</label>
            <select style={{ width: '100%', padding: 8, backgroundColor: '#334155', color: '#e2e8f0', border: '1px solid #475569', borderRadius: 4 }}>
              <option>Default Input</option>
            </select>
          </div>

          <div style={{ marginBottom: 16 }}>
            <label style={{ display: 'block', marginBottom: 8, color: '#94a3b8', fontSize: 12 }}>Audio Output</label>
            <select style={{ width: '100%', padding: 8, backgroundColor: '#334155', color: '#e2e8f0', border: '1px solid #475569', borderRadius: 4 }}>
              <option>Default Output</option>
            </select>
          </div>

          <div style={{ marginBottom: 24 }}>
            <label style={{ display: 'block', marginBottom: 8, color: '#94a3b8', fontSize: 12 }}>INPUT LEVEL</label>
            <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
              <input data-cy="input-level" type="range" min={-60} max={6} value={inputLevel} onChange={(e) => setInputLevel(Number(e.target.value))} style={{ flex: 1 }} />
              <div style={{ color: '#22c55e', fontWeight: 'bold', minWidth: 50 }}>{inputLevel} dB</div>
            </div>
          </div>

          <div style={{ marginBottom: 24 }}>
            <label style={{ display: 'block', marginBottom: 8, color: '#94a3b8', fontSize: 12 }}>OUTPUT LEVEL</label>
            <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
              <input data-cy="output-level" type="range" min={-60} max={6} value={outputLevel} onChange={(e) => setOutputLevel(Number(e.target.value))} style={{ flex: 1 }} />
              <div style={{ color: '#22c55e', fontWeight: 'bold', minWidth: 50 }}>{outputLevel} dB</div>
            </div>
          </div>
        </aside>

        {/* CENTER */}
        <section data-cy="signal-chain" style={{ flex: 1, backgroundColor: '#0f172a', padding: 24, overflowY: 'auto' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 12 }}>
            <h2 style={{ margin: 0, color: '#94a3b8', fontSize: 12, textTransform: 'uppercase' }}>Signal Chain</h2>
            <div>
              <button data-cy="add-effect-btn" onClick={() => setShowAddEffectModal(true)} style={{ padding: '8px 12px', backgroundColor: '#2563eb', color: '#fff', border: 'none', borderRadius: 4, cursor: 'pointer' }}>Add Effect</button>
            </div>
          </div>

          {/* Modal (simple) */}
          {showAddEffectModal && (
            <div data-cy="add-effect-modal" style={{ position: 'fixed', left: 0, top: 0, right: 0, bottom: 0, display: 'flex', alignItems: 'center', justifyContent: 'center', backgroundColor: 'rgba(0,0,0,0.5)' }}>
              <div style={{ backgroundColor: '#0f172a', padding: 20, borderRadius: 8, border: '1px solid #334155' }}>
                <h3 style={{ marginTop: 0 }}>Choose Effect</h3>
                <div style={{ display: 'flex', gap: 8 }}>
                  <button data-cy="effect-GAIN" onClick={() => addEffect('GAIN')}>GAIN</button>
                  <button data-cy="effect-OVERDRIVE" onClick={() => addEffect('OVERDRIVE')}>OVERDRIVE</button>
                  <button data-cy="effect-EQ" onClick={() => addEffect('EQ')}>EQ</button>
                </div>
                <div style={{ marginTop: 12 }}>
                  <button onClick={() => setShowAddEffectModal(false)}>Close</button>
                </div>
              </div>
            </div>
          )}

          {/* Scenes */}
          <div style={{ display: 'flex', gap: 8, marginBottom: 12 }}>
            {[1, 2, 3].map(n => (
              <button key={n} data-cy={`scene-btn-${n}`} className={currentScene === n ? 'active' : ''} onClick={() => switchScene(n)}>{`Scene ${n}`}</button>
            ))}
          </div>

          {/* Signal chain blocks */}
          <div style={{ display: 'flex', gap: 12, flexWrap: 'wrap' }}>
            {(scenes[currentScene] || []).map(item => (
              <div key={item.id} draggable data-id={item.id} data-cy="signal-chain-block" onDragStart={(e) => onDragStart(e, item.id)} onDragOver={onDragOver} onDrop={(e) => onDrop(e, item.id)} style={{ padding: 12, backgroundColor: '#334155', borderRadius: 6 }}>
                <div style={{ display: 'flex', gap: 8, alignItems: 'center' }}>
                  <div style={{ fontWeight: 'bold' }}>{item.type}</div>
                  <button data-cy="effect-remove-btn" onClick={() => removeEffect(item.id)} style={{ marginLeft: 8 }}>Remove</button>
                </div>
              </div>
            ))}
          </div>

        </section>

        {/* RIGHT */}
        <aside data-cy="right-panel" style={{ width: 280, backgroundColor: '#1a202c', borderLeft: '1px solid #334155', padding: 16, overflowY: 'auto' }}>
          <h3 style={{ marginBottom: 12, color: '#94a3b8', fontSize: 12, textTransform: 'uppercase' }}>MASTER VOLUME</h3>

          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', gap: 12 }}>
            <div data-cy="master-volume">
              <div data-cy="master-volume-knob">
                <RotaryKnob value={masterVolume} onChange={(v) => setMasterVolume(v)} min={-60} max={6} step={1} size={92} accentColor="#22c55e" showValue={false} />
              </div>
              <input data-cy="master-volume-input" type="number" value={masterVolume} onChange={(e) => setMasterVolume(Number(e.target.value))} style={{ position: 'absolute', left: -9999, width: 1, height: 1, opacity: 0 }} />
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center' }}>
              <div data-cy="master-volume-display" style={{ color: '#22c55e', fontWeight: 'bold' }}>{masterVolume} dB</div>
              <button style={{ padding: '8px', backgroundColor: '#7f1d1d', color: '#fff', border: 'none', borderRadius: 4 }}>MUTE</button>
            </div>
          </div>

          <h3 style={{ marginTop: 20, marginBottom: 12, color: '#94a3b8', fontSize: 12, textTransform: 'uppercase' }}>OUTPUT METER</h3>
          <div data-cy="cpu-graph" style={{ height: 120, backgroundColor: '#0f172a', border: '1px solid #334155', borderRadius: 4, padding: 8 }} />
        </aside>

      </main>

      {/* FOOTER */}
      <footer style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '12px 24px', backgroundColor: '#1e293b', borderTop: '1px solid #334155', fontSize: 12 }}>
        <div>TAP | 120.0 BPM</div>
        <div style={{ color: '#94a3b8' }}>🎵 METRONOME: OFF</div>
        <input type="text" placeholder="Write your note here..." style={{ flex: 1, margin: '0 24px', padding: '6px 12px', backgroundColor: '#334155', border: '1px solid #475569', borderRadius: 4, color: '#e2e8f0' }} />
        <div style={{ color: '#94a3b8' }}>CPU: 6%</div>
      </footer>
    </div>
  );
}

export default PerformViewSimplified;
