import React from 'react';

export interface CardProps {
  title?: string;
  children: React.ReactNode;
  footer?: React.ReactNode;
  onRemove?: () => void;
  className?: string;
}

export const Card: React.FC<CardProps> = ({ title, children, footer, onRemove, className = '' }) => {
  return (
    <div className={`card ${className}`}>
      {title && (
        <div className="flex justify-between items-center mb-4 pb-4 border-b border-gray-200 dark:border-gray-700">
          <h3 className="text-lg font-semibold text-gray-900 dark:text-white">{title}</h3>
          {onRemove && (
            <button
              onClick={onRemove}
              className="text-gray-500 hover:text-gray-700 dark:hover:text-gray-300 text-xl"
            >
              ×
            </button>
          )}
        </div>
      )}
      <div className="mb-4">{children}</div>
      {footer && <div className="pt-4 border-t border-gray-200 dark:border-gray-700">{footer}</div>}
    </div>
  );
};
