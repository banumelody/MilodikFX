import React, { useState } from 'react';
import { Input, Button } from './index';

export interface LibraryTabProps {
  presets?: Array<{ id: string; name: string; tags?: string[] }>;
  scenes?: Array<{ id: string; name: string }>;
  onLoadPreset?: (id: string) => void;
  onDeletePreset?: (id: string) => void;
  onLoadScene?: (id: string) => void;
}

export const LibraryTab: React.FC<LibraryTabProps> = ({
  presets = [],
  scenes = [],
  onLoadPreset,
  onDeletePreset,
  onLoadScene,
}) => {
  const [searchTerm, setSearchTerm] = useState('');

  const filteredPresets = presets.filter((p) =>
    p.name.toLowerCase().includes(searchTerm.toLowerCase())
  );

  return (
    <div className="p-6 flex flex-col gap-6">
      {/* Presets */}
      <div className="rounded-lg bg-gray-800 border border-gray-700 p-4">
        <h2 className="text-2xl font-bold text-white mb-4">Presets</h2>
        <Input
          type="text"
          placeholder="Search presets..."
          value={searchTerm}
          onChange={(e) => setSearchTerm(e.target.value)}
          className="mb-4"
        />
        <div className="space-y-2 max-h-96 overflow-y-auto">
          {filteredPresets.length > 0 ? (
            filteredPresets.map((preset) => (
              <div
                key={preset.id}
                className="flex items-center justify-between p-3 rounded bg-gray-700 hover:bg-gray-600 transition-colors"
              >
                <div>
                  <h3 className="font-semibold text-white">{preset.name}</h3>
                  {preset.tags && (
                    <div className="flex gap-1 mt-1">
                      {preset.tags.map((tag) => (
                        <span key={tag} className="text-xs bg-cyan-700 px-2 py-1 rounded">
                          {tag}
                        </span>
                      ))}
                    </div>
                  )}
                </div>
                <div className="flex gap-2">
                  <Button
                    size="sm"
                    onClick={() => onLoadPreset?.(preset.id)}
                  >
                    Load
                  </Button>
                  <Button
                    size="sm"
                    variant="danger"
                    onClick={() => onDeletePreset?.(preset.id)}
                  >
                    Delete
                  </Button>
                </div>
              </div>
            ))
          ) : (
            <div className="text-gray-500 text-center py-8">No presets found</div>
          )}
        </div>
      </div>

      {/* Scenes */}
      {scenes.length > 0 && (
        <div className="rounded-lg bg-gray-800 border border-gray-700 p-4">
          <h2 className="text-2xl font-bold text-white mb-4">Scenes</h2>
          <div className="space-y-2 max-h-96 overflow-y-auto">
            {scenes.map((scene) => (
              <div
                key={scene.id}
                className="flex items-center justify-between p-3 rounded bg-gray-700 hover:bg-gray-600 transition-colors"
              >
                <h3 className="font-semibold text-white">{scene.name}</h3>
                <Button
                  size="sm"
                  onClick={() => onLoadScene?.(scene.id)}
                >
                  Load
                </Button>
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
};
