import React from 'react';

export interface MainLayoutProps {
  topBar?: React.ReactNode;
  leftPanel?: React.ReactNode;
  rightPanel?: React.ReactNode;
  children: React.ReactNode;
}

export const MainLayout: React.FC<MainLayoutProps> = ({
  topBar,
  leftPanel,
  rightPanel,
  children,
}) => {
  return (
    <div className="flex flex-col h-screen bg-gray-950 text-white">
      {/* Top Bar */}
      {topBar && <div className="flex-shrink-0">{topBar}</div>}

      {/* Main Content Area */}
      <div className="flex flex-1 overflow-hidden gap-4 p-4">
        {/* Left Panel */}
        {leftPanel && <div className="flex-shrink-0 overflow-y-auto">{leftPanel}</div>}

        {/* Center Content */}
        <div className="flex-1 overflow-y-auto rounded-lg bg-gray-800 border border-gray-700">
          {children}
        </div>

        {/* Right Panel */}
        {rightPanel && <div className="flex-shrink-0 overflow-y-auto">{rightPanel}</div>}
      </div>
    </div>
  );
};
