export interface ToggleProps {
  checked: boolean;
  onChange: (checked: boolean) => void;
  label?: string;
  accent?: string;
  disabled?: boolean;
}

export function Toggle({ checked, onChange, label, accent = '#4da3ff', disabled = false }: ToggleProps) {
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

export default Toggle;
