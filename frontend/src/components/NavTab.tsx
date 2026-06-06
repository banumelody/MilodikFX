import React from 'react';

export interface NavTabProps {
  label: string;
  active: boolean;
  onClick: () => void;
  icon?: React.ReactNode;
}

export const NavTab: React.FC<NavTabProps> = ({
  label,
  active,
  onClick,
  icon,
}) => {
  return (
    <button
      onClick={onClick}
      className={`
        flex items-center gap-2 px-6 py-3 font-medium
        transition-all duration-200 border-b-2
        ${
          active
            ? 'border-cyan-500 text-cyan-400'
            : 'border-transparent text-gray-400 hover:text-cyan-300'
        }
      `}
    >
      {icon && <span>{icon}</span>}
      <span>{label}</span>
    </button>
  );
};
