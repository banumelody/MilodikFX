import React from 'react';

export interface ToggleSwitchProps {
  checked: boolean;
  onChange: (checked: boolean) => void;
  disabled?: boolean;
  label?: string;
  size?: 'sm' | 'md' | 'lg';
}

export const ToggleSwitch: React.FC<ToggleSwitchProps> = ({
  checked,
  onChange,
  disabled = false,
  label,
  size = 'md',
}) => {
  const sizeClasses = {
    sm: 'w-8 h-4',
    md: 'w-12 h-6',
    lg: 'w-16 h-8',
  };

  const toggleClasses = {
    sm: 'w-3 h-3',
    md: 'w-5 h-5',
    lg: 'w-7 h-7',
  };

  const translateClasses = {
    sm: checked ? 'translate-x-4' : 'translate-x-0.5',
    md: checked ? 'translate-x-6' : 'translate-x-0.5',
    lg: checked ? 'translate-x-8' : 'translate-x-0.5',
  };

  const handleClick = () => {
    if (!disabled) {
      onChange(!checked);
    }
  };

  return (
    <div className="flex items-center gap-2">
      <button
        onClick={handleClick}
        disabled={disabled}
        aria-pressed={checked}
        className={`
          ${sizeClasses[size]}
          ${checked ? 'bg-cyan-500' : 'bg-gray-300 dark:bg-gray-600'}
          rounded-full transition-colors duration-200
          disabled:opacity-50 disabled:cursor-not-allowed
          focus:outline-none focus:ring-2 focus:ring-cyan-400
          relative
        `}
      >
        <div
          className={`
            ${toggleClasses[size]}
            ${translateClasses[size]}
            bg-white rounded-full
            absolute top-0.5
            transition-transform duration-200
          `}
        />
      </button>
      {label && <label className="text-sm font-medium dark:text-white">{label}</label>}
    </div>
  );
};
