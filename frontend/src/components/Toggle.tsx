import { memo } from 'react';

export interface ToggleProps {
  checked: boolean;
  onChange: (checked: boolean) => void;
  label?: string;
  accent?: string;
  disabled?: boolean;
}

function ToggleBase({ checked, onChange, label, accent = '#4da3ff', disabled = false }: ToggleProps) {
  return (
    <button
      type="button"
      role="switch"
      aria-checked={checked}
      aria-label={label}
      disabled={disabled}
      className={`toggle${checked ? ' toggle--on' : ''}`}
      style={checked ? { backgroundColor: accent, borderColor: accent } : undefined}
      onClick={() => onChange(!checked)}
    >
      <span className="toggle__thumb" />
    </button>
  );
}

/** Memoised for the same reason as Knob: many instances, re-rendered at 22 Hz. */
export const Toggle = memo(ToggleBase);

export default Toggle;
