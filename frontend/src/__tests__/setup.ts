import '@testing-library/jest-dom';
import { beforeEach, vi } from 'vitest';

// jsdom has no pointer capture, which the knob's drag handling touches.
if (!Element.prototype.setPointerCapture) {
  Element.prototype.setPointerCapture = vi.fn();
  Element.prototype.releasePointerCapture = vi.fn();
  Element.prototype.hasPointerCapture = vi.fn(() => false);
}

beforeEach(() => {
  vi.clearAllMocks();
  localStorage.clear();
});
