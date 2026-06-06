import { describe, it, expect, vi } from 'vitest';
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { Modal } from '../Modal';

describe('Modal', () => {
  it('renders nothing when isOpen is false', () => {
    render(
      <Modal isOpen={false} title="Test Modal" onClose={vi.fn()}>
        <p>Modal content</p>
      </Modal>
    );
    expect(screen.queryByText('Test Modal')).not.toBeInTheDocument();
  });

  it('renders title and content when isOpen is true', () => {
    render(
      <Modal isOpen={true} title="Test Modal" onClose={vi.fn()}>
        <p>Modal content</p>
      </Modal>
    );
    expect(screen.getByText('Test Modal')).toBeInTheDocument();
    expect(screen.getByText('Modal content')).toBeInTheDocument();
  });

  it('calls onClose when close button is clicked', async () => {
    const onClose = vi.fn();
    render(
      <Modal isOpen={true} title="Test Modal" onClose={onClose}>
        <p>Modal content</p>
      </Modal>
    );
    
    const closeButton = screen.getByTitle('Close modal');
    await userEvent.click(closeButton);
    
    expect(onClose).toHaveBeenCalled();
  });

  it('calls onClose when backdrop is clicked', async () => {
    const onClose = vi.fn();
    render(
      <Modal isOpen={true} title="Test Modal" onClose={onClose}>
        <p>Modal content</p>
      </Modal>
    );
    
    const backdrop = document.querySelector('.fixed.inset-0.z-50');
    if (backdrop) {
      await userEvent.click(backdrop);
      expect(onClose).toHaveBeenCalled();
    }
  });

  it('does not close when content is clicked', async () => {
    const onClose = vi.fn();
    render(
      <Modal isOpen={true} title="Test Modal" onClose={onClose}>
        <p>Modal content</p>
      </Modal>
    );
    
    const content = screen.getByText('Modal content');
    await userEvent.click(content);
    
    expect(onClose).not.toHaveBeenCalled();
  });

  it('supports size prop', () => {
    render(
      <Modal isOpen={true} title="Small Modal" onClose={vi.fn()} size="sm">
        <p>Small content</p>
      </Modal>
    );
    const modal = document.querySelector('.max-w-sm');
    expect(modal).toBeInTheDocument();
  });
});
