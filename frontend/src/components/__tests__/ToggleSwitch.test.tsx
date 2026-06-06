import { describe, it, expect, vi } from 'vitest';
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { ToggleSwitch } from '../ToggleSwitch';

describe('ToggleSwitch', () => {
  it('renders with initial unchecked state', () => {
    render(<ToggleSwitch checked={false} onChange={vi.fn()} />);
    const button = screen.getByRole('button');
    expect(button).toHaveAttribute('aria-pressed', 'false');
  });

  it('renders with initial checked state', () => {
    render(<ToggleSwitch checked={true} onChange={vi.fn()} />);
    const button = screen.getByRole('button');
    expect(button).toHaveAttribute('aria-pressed', 'true');
  });

  it('calls onChange when clicked', async () => {
    const onChange = vi.fn();
    render(<ToggleSwitch checked={false} onChange={onChange} />);
    
    const button = screen.getByRole('button');
    await userEvent.click(button);
    
    expect(onChange).toHaveBeenCalledWith(true);
  });

  it('supports size prop', () => {
    render(<ToggleSwitch checked={false} onChange={vi.fn()} size="lg" />);
    const button = screen.getByRole('button');
    expect(button).toBeInTheDocument();
  });

  it('can be disabled', async () => {
    const onChange = vi.fn();
    render(<ToggleSwitch checked={false} onChange={onChange} disabled={true} />);
    
    const button = screen.getByRole('button');
    await userEvent.click(button);
    
    expect(onChange).not.toHaveBeenCalled();
  });

  it('renders label when provided', () => {
    render(<ToggleSwitch checked={false} onChange={vi.fn()} label="Test Label" />);
    expect(screen.getByText('Test Label')).toBeInTheDocument();
  });
});
