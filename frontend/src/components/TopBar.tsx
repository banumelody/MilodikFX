import React from 'react';
import { NavTab, Button } from './index';

export interface TopBarProps {
  title?: string;
  version?: string;
  tabs?: Array<{ id: string; label: string; icon?: string }>;
  activeTab?: string;
  onTabChange?: (tabId: string) => void;
  presetName?: string;
  onSave?: () => void;
  onSaveAs?: () => void;
  onImport?: () => void;
  onExport?: () => void;
}

export const TopBar: React.FC<TopBarProps> = ({
  title = 'MilodikFX',
  version = 'v1.0.0',
  tabs = [],
  activeTab,
  onTabChange,
  presetName = 'Untitled',
  onSave,
  onSaveAs,
  onImport,
  onExport,
}) => {
  return (
    <div className="bg-gray-900 border-b border-gray-700 sticky top-0 z-40">
      {/* Header */}
      <div className="flex items-center justify-between px-6 py-3 border-b border-gray-700">
        <div className="flex items-center gap-4">
          <h1 className="text-2xl font-bold text-cyan-400">{title}</h1>
          <span className="text-xs text-gray-500">{version}</span>
        </div>
        <div className="flex items-center gap-2">
          <span className="text-sm text-gray-400">Preset:</span>
          <span className="font-mono text-white">{presetName}</span>
          {onSave && <Button size="sm" onClick={onSave}>Save</Button>}
          {onSaveAs && <Button size="sm" variant="secondary" onClick={onSaveAs}>Save As</Button>}
          {onImport && <Button size="sm" variant="secondary" onClick={onImport}>Import</Button>}
          {onExport && <Button size="sm" variant="secondary" onClick={onExport}>Export</Button>}
        </div>
      </div>

      {/* Tabs */}
      {tabs.length > 0 && (
        <div className="flex gap-1 px-6">
          {tabs.map((tab) => (
            <NavTab
              key={tab.id}
              label={tab.label}
              icon={tab.icon}
              active={activeTab === tab.id}
              onClick={() => onTabChange?.(tab.id)}
            />
          ))}
        </div>
      )}
    </div>
  );
};
