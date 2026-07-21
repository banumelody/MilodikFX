import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { ApiError, getEffects, setParameter, subscribeLevels } from '../api';

function jsonResponse(body: unknown, status = 200) {
  return {
    ok: status >= 200 && status < 300,
    status,
    statusText: status === 200 ? 'OK' : 'Error',
    json: async () => body,
  } as Response;
}

describe('api client', () => {
  beforeEach(() => {
    vi.stubGlobal('fetch', vi.fn());
  });

  afterEach(() => {
    vi.unstubAllGlobals();
    vi.useRealTimers();
  });

  it('addresses the engine relative to the page origin', async () => {
    // The engine falls back to ports 3001+ when 3000 is taken, so a hardcoded
    // port broke precisely when the fallback kicked in.
    vi.mocked(fetch).mockResolvedValue(jsonResponse({ effects: [] }));

    await getEffects();

    expect(fetch).toHaveBeenCalledWith(
      `${window.location.origin}/api/effects`,
      expect.objectContaining({ headers: { 'Content-Type': 'application/json' } }),
    );
  });

  it('sends parameter writes as a PUT with a numeric value', async () => {
    vi.mocked(fetch).mockResolvedValue(
      jsonResponse({ effect: 'reverb', parameter: 'mixPct', value: 30 }),
    );

    await setParameter('reverb', 'mixPct', 30);

    const [url, init] = vi.mocked(fetch).mock.calls[0];
    expect(url).toContain('/api/effects/reverb/mixPct');
    expect(init?.method).toBe('PUT');
    expect(JSON.parse(String(init?.body))).toEqual({ value: 30 });
  });

  it('escapes ids so a name cannot alter the path', async () => {
    vi.mocked(fetch).mockResolvedValue(jsonResponse({}));

    await setParameter('a/b', 'c d', 1);

    expect(String(vi.mocked(fetch).mock.calls[0][0])).toContain('/api/effects/a%2Fb/c%20d');
  });

  it('surfaces the engine error message rather than the status code', async () => {
    vi.mocked(fetch).mockResolvedValue(
      jsonResponse({ error: "Couldn't open the output device (buffer size mismatch)" }, 400),
    );

    await expect(getEffects()).rejects.toThrowError(ApiError);
    await expect(getEffects()).rejects.toThrow(/buffer size mismatch/);
  });

  it('falls back to the status text when the error body is not JSON', async () => {
    vi.mocked(fetch).mockResolvedValue({
      ok: false,
      status: 500,
      statusText: 'Internal Server Error',
      json: async () => {
        throw new Error('not json');
      },
    } as unknown as Response);

    await expect(getEffects()).rejects.toThrow('Internal Server Error');
  });

  it('does not stack level requests when the engine is slow', async () => {
    vi.useFakeTimers();

    const pending: Array<(value: Response) => void> = [];
    vi.mocked(fetch).mockImplementation(
      () =>
        new Promise<Response>((resolve) => {
          pending.push(resolve);
        }),
    );

    const stop = subscribeLevels(
      () => undefined,
      () => undefined,
      10,
    );

    await vi.advanceTimersByTimeAsync(100);

    // Ten ticks elapsed, but the first response never arrived, so exactly one
    // request should be outstanding.
    expect(fetch).toHaveBeenCalledTimes(1);

    pending.forEach((resolve) => resolve(jsonResponse({})));
    stop();
  });

  it('stops polling once unsubscribed', async () => {
    vi.useFakeTimers();
    vi.mocked(fetch).mockResolvedValue(jsonResponse({}));

    const stop = subscribeLevels(
      () => undefined,
      () => undefined,
      10,
    );

    await vi.advanceTimersByTimeAsync(25);
    const callsBefore = vi.mocked(fetch).mock.calls.length;

    stop();
    await vi.advanceTimersByTimeAsync(100);

    expect(vi.mocked(fetch).mock.calls.length).toBe(callsBefore);
  });
});
