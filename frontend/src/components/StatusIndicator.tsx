import React from 'react';

export type StatusType = 'idle' | 'active' | 'warning' | 'error';

export interface StatusIndicatorProps {
  status: StatusType;
  label: string;
  color?: string;
}

const statusColorMap: Record<StatusType, string> = {
  idle: 'bg-gray-500',
  active: 'bg-green-500',
  warning: 'bg-amber-500',
  error: 'bg-red-500',
};

export const StatusIndicator: React.FC<StatusIndicatorProps> = ({
  status,
  label,
  color,
}) => {
  const bgColor = color || statusColorMap[status];

  return (
    <div className="flex items-center gap-2">
      <div
        className={`
          w-3 h-3 rounded-full ${bgColor}
          animate-pulse
        `}
      />
      <span className="text-sm font-medium text-gray-300">{label}</span>
    </div>
  );
};
