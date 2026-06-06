import React, { useState, useEffect } from 'react';
import {
  MainLayout,
  TopBar,
  LeftPanel,
  RightPanel,
  NavTab,
  PerformTab,
  EditTab,
  LibraryTab,
  SettingsTab,
  Effect,
} from './index';

export interface DashboardV2Props {
  // Optional props for testing/customization
}

type TabType = 'perform' | 'edit' | 'library' | 'settings';

export const DashboardV2: React.FC<DashboardV2Props> = () => {
  const [activeTab, setActiveTab] = useState<TabType>('perform');
  const [effects, setEffects] = useState<Effect[]>([]);
  const [selectedEffectId, setSelectedEffectId] = useState<string | null>(null);
  const [scenes] = useState([
    { id: 1, name: 'Clean' },
    { id: 2, name: 'Crunch' },
    { id: 3, name: 'Lead' },
    { id: 4, name: 'Custom' },
  ]);

  // Simulate audio metrics updates
  const [audioMetrics, setAudioMetrics] = useState({
    inputLevel: -40,
    outputLevel: -40,
    cpuLoad: 0,
    masterVolume: 75,
    isMuted: false,
  });

  useEffect(() => {
    const interval = setInterval(() => {
      setAudioMetrics((prev) => ({
        ...prev,
        inputLevel: -60 + Math.random() * 20,
        outputLevel: -60 + Math.random() * 20,
        cpuLoad: Math.random() * 30,
      }));
    }, 200);
    return () => clearInterval(interval);
  }, []);

  const handleAddEffect = (effectType: string) => {
    const newEffect: Effect = {
      id: `effect-${Date.now()}`,
      name: effectType.charAt(0).toUpperCase() + effectType.slice(1).toLowerCase(),
      type: effectType,
      position: effects.length,
    };
    setEffects([...effects, newEffect]);
  };

  const handleRemoveEffect = (effectId: string) => {
    setEffects(effects.filter((e) => e.id !== effectId));
    if (selectedEffectId === effectId) {
      setSelectedEffectId(null);
    }
  };

  const handleReorderEffects = (newOrder: string[]) => {
    const reorderedEffects = newOrder
      .map((id) => effects.find((e) => e.id === id))
      .filter((e): e is Effect => e !== undefined);
    setEffects(reorderedEffects);
  };

  const renderTabContent = () => {
    switch (activeTab) {
      case 'perform':
        return (
          <PerformTab
            effects={effects}
            scenes={scenes}
            onAddEffect={handleAddEffect}
            onRemoveEffect={handleRemoveEffect}
            onReorderEffects={handleReorderEffects}
            onSceneSelect={(sceneId) => console.log(`Switched to scene: ${sceneId}`)}
          />
        );
      case 'edit':
        return (
          <EditTab
            selectedEffect={effects.find((e) => e.id === selectedEffectId)}
            onParameterChange={(name, value) => {
              console.log(`Changed ${name} to ${value}`);
            }}
          />
        );
      case 'library':
        return <LibraryTab />;
      case 'settings':
        return <SettingsTab />;
      default:
        return null;
    }
  };

  return (
    <MainLayout
      topBar={
        <TopBar
          presetName="Factory: Clean"
          version="v0.8.0"
          onSave={() => console.log('Save preset')}
          onSaveAs={() => console.log('Save preset as')}
        />
      }
      leftPanel={
        <LeftPanel
          inputLevel={audioMetrics.inputLevel}
          outputLevel={audioMetrics.outputLevel}
          tunerFrequency={440}
          scenes={scenes}
          onSceneSelect={(sceneId) => console.log(`Selected scene: ${sceneId}`)}
          onTapTempo={() => console.log('Tap tempo')}
        />
      }
      rightPanel={
        <RightPanel
          masterVolume={audioMetrics.masterVolume}
          isMuted={audioMetrics.isMuted}
          cpuLoad={audioMetrics.cpuLoad}
          cpuHistory={Array.from({ length: 30 }, () => Math.random() * 30)}
          outputLevel={audioMetrics.outputLevel}
          notes="All systems operational"
        />
      }
    >
      {/* Tab Navigation */}
      <div className="flex gap-2 p-4 border-b border-gray-700">
        <NavTab
          label="Perform"
          active={activeTab === 'perform'}
          onClick={() => setActiveTab('perform')}
        />
        <NavTab
          label="Edit"
          active={activeTab === 'edit'}
          onClick={() => setActiveTab('edit')}
        />
        <NavTab
          label="Library"
          active={activeTab === 'library'}
          onClick={() => setActiveTab('library')}
        />
        <NavTab
          label="Settings"
          active={activeTab === 'settings'}
          onClick={() => setActiveTab('settings')}
        />
      </div>

      {/* Tab Content */}
      <div className="flex-1 overflow-y-auto">{renderTabContent()}</div>
    </MainLayout>
  );
};

export default DashboardV2;
