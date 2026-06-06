import { describe, it, expect, vi } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import { Button } from '../../components/Button';

describe('Button Component', () => {
  it('renders with text', () => {
    render(<Button>Click me</Button>);
    expect(screen.getByText('Click me')).toBeInTheDocument();
  });

  it('calls onClick handler when clicked', () => {
    const onClick = vi.fn();
    render(<Button onClick={onClick}>Click me</Button>);

    fireEvent.click(screen.getByText('Click me'));
    expect(onClick).toHaveBeenCalled();
  });

  it('applies primary variant styles', () => {
    const { container } = render(<Button variant="primary">Primary</Button>);
    const button = container.querySelector('button');

    expect(button).toHaveClass('bg-primary-600');
  });

  it('applies secondary variant styles', () => {
    const { container } = render(<Button variant="secondary">Secondary</Button>);
    const button = container.querySelector('button');

    expect(button).toHaveClass('bg-gray-200');
  });

  it('disables button when disabled prop is true', () => {
    const { container } = render(<Button disabled>Disabled</Button>);
    const button = container.querySelector('button') as HTMLButtonElement;

    expect(button.disabled).toBe(true);
  });

  it('applies correct size classes', () => {
    const { container: smallContainer } = render(
      <Button size="sm">Small</Button>
    );
    expect(smallContainer.querySelector('button')).toHaveClass('px-3');

    const { container: largeContainer } = render(
      <Button size="lg">Large</Button>
    );
    expect(largeContainer.querySelector('button')).toHaveClass('px-6');
  });
});
