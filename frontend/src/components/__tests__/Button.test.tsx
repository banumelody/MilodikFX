import { describe, it, expect, vi } from 'vitest';
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { Button } from '../Button';

describe('Button', () => {
  it('renders with text content', () => {
    render(<Button>Click me</Button>);
    expect(screen.getByText('Click me')).toBeInTheDocument();
  });

  it('calls onClick handler when clicked', async () => {
    const onClick = vi.fn();
    render(<Button onClick={onClick}>Click me</Button>);
    
    await userEvent.click(screen.getByText('Click me'));
    expect(onClick).toHaveBeenCalled();
  });

  it('supports primary variant', () => {
    render(<Button variant="primary">Primary</Button>);
    const button = screen.getByText('Primary');
    expect(button.className).toContain('bg-primary-600');
  });

  it('supports secondary variant', () => {
    render(<Button variant="secondary">Secondary</Button>);
    const button = screen.getByText('Secondary');
    expect(button.className).toContain('bg-gray-200');
  });

  it('supports danger variant', () => {
    render(<Button variant="danger">Delete</Button>);
    const button = screen.getByText('Delete');
    expect(button.className).toContain('bg-error-600');
  });

  it('supports size prop', () => {
    render(<Button size="lg">Large Button</Button>);
    const button = screen.getByText('Large Button');
    expect(button).toBeInTheDocument();
  });

  it('can be disabled', async () => {
    const onClick = vi.fn();
    render(
      <Button disabled onClick={onClick}>
        Click me
      </Button>
    );
    
    const button = screen.getByText('Click me');
    expect(button).toBeDisabled();
    
    await userEvent.click(button);
    expect(onClick).not.toHaveBeenCalled();
  });

  it('renders children content correctly', () => {
    render(
      <Button>
        <span>Custom Content</span>
      </Button>
    );
    expect(screen.getByText('Custom Content')).toBeInTheDocument();
  });
});
