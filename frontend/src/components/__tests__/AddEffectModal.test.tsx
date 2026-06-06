import { describe, it, expect, vi } from 'vitest';
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { AddEffectModal } from '../AddEffectModal';

describe('AddEffectModal', () => {
  it('does not render when isOpen is false', () => {
    render(
      <AddEffectModal isOpen={false} onClose={vi.fn()} onSelectEffect={vi.fn()} />
    );
    expect(screen.queryByText('Add Effect')).not.toBeInTheDocument();
  });

  it('renders all effect options when open', () => {
    render(
      <AddEffectModal isOpen={true} onClose={vi.fn()} onSelectEffect={vi.fn()} />
    );
    expect(screen.getByText('Add Effect')).toBeInTheDocument();
    expect(screen.getByText('Gain')).toBeInTheDocument();
    expect(screen.getByText('Overdrive')).toBeInTheDocument();
    expect(screen.getByText('Equalizer')).toBeInTheDocument();
    expect(screen.getByText('Compressor')).toBeInTheDocument();
    expect(screen.getByText('Noise Gate')).toBeInTheDocument();
    expect(screen.getByText('Delay')).toBeInTheDocument();
    expect(screen.getByText('Reverb')).toBeInTheDocument();
  });

  it('calls onSelectEffect with effect ID when effect is selected', async () => {
    const onSelectEffect = vi.fn();
    render(
      <AddEffectModal isOpen={true} onClose={vi.fn()} onSelectEffect={onSelectEffect} />
    );
    
    const gainButton = screen.getByText('Gain').closest('button');
    if (gainButton) {
      await userEvent.click(gainButton);
      expect(onSelectEffect).toHaveBeenCalledWith('GAIN');
    }
  });

  it('calls onClose after selecting an effect', async () => {
    const onClose = vi.fn();
    render(
      <AddEffectModal isOpen={true} onClose={onClose} onSelectEffect={vi.fn()} />
    );
    
    const overdrive = screen.getByText('Overdrive').closest('button');
    if (overdrive) {
      await userEvent.click(overdrive);
      expect(onClose).toHaveBeenCalled();
    }
  });

  it('displays effect descriptions', () => {
    render(
      <AddEffectModal isOpen={true} onClose={vi.fn()} onSelectEffect={vi.fn()} />
    );
    expect(screen.getByText('Adjustable input/output level')).toBeInTheDocument();
    expect(screen.getByText('Warm, musical distortion')).toBeInTheDocument();
    expect(screen.getByText('3-band parametric EQ')).toBeInTheDocument();
    expect(screen.getByText('Dynamic range control')).toBeInTheDocument();
    expect(screen.getByText('Silence quiet signals')).toBeInTheDocument();
    expect(screen.getByText('Time-based echo effect')).toBeInTheDocument();
    expect(screen.getByText('Spatial ambience effect')).toBeInTheDocument();
  });

  it('can close the modal via close button', async () => {
    const onClose = vi.fn();
    render(
      <AddEffectModal isOpen={true} onClose={onClose} onSelectEffect={vi.fn()} />
    );
    
    const closeButton = screen.getByTitle('Close modal');
    await userEvent.click(closeButton);
    
    expect(onClose).toHaveBeenCalled();
  });
});
