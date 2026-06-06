import React from 'react';

export interface LabeledValueProps {
  label: string;
  value: string | number;
  units?: string;
  color?: 'default' | 'cyan' | 'amber' | 'green' | 'red';
  size?: 'sm' | 'md' | 'lg';
  tooltip?: string;
}

export const LabeledValue: React.FC<LabeledValueProps> = ({
  label,
  value,
  units,
  color = 'default',
  size = 'md',
  tooltip,
}) => {
  const colorClasses = {
    default: 'text-gray-900 dark:text-white',
    cyan: 'text-cyan-500',
    amber: 'text-amber-500',
    green: 'text-green-500',
    red: 'text-red-500',
  };

  const sizeClasses = {
    sm: { label: 'text-xs', value: 'text-sm' },
    md: { label: 'text-sm', value: 'text-base' },
    lg: { label: 'text-base', value: 'text-lg' },
  };

  return (
    <div
      className="flex flex-col gap-1"
      title={tooltip}
    >
      <label className={`text-gray-600 dark:text-gray-400 font-medium ${sizeClasses[size].label}`}>
        {label}
      </label>
      <div className={`${colorClasses[color]} font-semibold ${sizeClasses[size].value} flex items-baseline gap-1`}>
        <span>{value}</span>
        {units && <span className="text-xs opacity-75">{units}</span>}
      </div>
    </div>
  );
};
