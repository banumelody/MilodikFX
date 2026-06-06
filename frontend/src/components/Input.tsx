import React from 'react';

export interface InputProps extends React.InputHTMLAttributes<HTMLInputElement> {
  label?: string;
  error?: string;
}

export const Input: React.FC<InputProps> = ({ label, error, className = '', ...props }) => {
  return (
    <div className="flex flex-col gap-1">
      {label && <label className="label">{label}</label>}
      <input
        {...props}
        className={`input ${error ? 'border-error-500' : ''} ${className}`}
      />
      {error && <span className="text-xs text-error-600">{error}</span>}
    </div>
  );
};
