import React from 'react';

export interface LabelProps {
  text: string;
  htmlFor?: string;
  tooltip?: string;
  className?: string;
}

export const Label: React.FC<LabelProps> = ({ text, htmlFor, tooltip, className = '' }) => {
  return (
    <label
      htmlFor={htmlFor}
      className={`label ${className}`}
      title={tooltip}
    >
      {text}
      {tooltip && (
        <span className="ml-1 text-gray-400 dark:text-gray-500 cursor-help">?</span>
      )}
    </label>
  );
};
