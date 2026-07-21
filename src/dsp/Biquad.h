#pragma once

#include <JuceHeader.h>

#include <cmath>

namespace milodikfx::dsp
{
/**
 * Direct-form-I biquad coefficients, already normalised by a0.
 *
 * These are plain floats on purpose: every processor recomputes them on the
 * audio thread when (and only when) a parameter actually changed, so they are
 * owned exclusively by the audio thread and never need to be published across
 * threads. That removes both the heap allocation and the data race that the
 * juce::dsp::IIR::Coefficients::Ptr approach had here.
 */
struct BiquadCoeffs
{
    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
};

struct BiquadState
{
    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;

    void reset() noexcept
    {
        x1 = x2 = y1 = y2 = 0.0f;
    }

    inline float process (const BiquadCoeffs& c, float in) noexcept
    {
        const auto out = c.b0 * in + c.b1 * x1 + c.b2 * x2 - c.a1 * y1 - c.a2 * y2;

        x2 = x1;
        x1 = in;
        y2 = y1;
        y1 = out;

        return out;
    }
};

namespace biquad
{
/** Keeps w0 strictly inside (0, pi) so the RBJ formulas stay well defined. */
inline double safeOmega (double freqHz, double sampleRate) noexcept
{
    const auto nyquistLimit = juce::jmax (1.0, sampleRate * 0.45);
    const auto f = juce::jlimit (1.0, nyquistLimit, freqHz);
    return juce::MathConstants<double>::twoPi * f / juce::jmax (1.0, sampleRate);
}

inline BiquadCoeffs normalise (double b0, double b1, double b2, double a0, double a1, double a2) noexcept
{
    if (! std::isfinite (a0) || std::abs (a0) < 1.0e-12)
        return {};

    BiquadCoeffs c;
    c.b0 = (float) (b0 / a0);
    c.b1 = (float) (b1 / a0);
    c.b2 = (float) (b2 / a0);
    c.a1 = (float) (a1 / a0);
    c.a2 = (float) (a2 / a0);

    if (! std::isfinite (c.b0) || ! std::isfinite (c.b1) || ! std::isfinite (c.b2)
        || ! std::isfinite (c.a1) || ! std::isfinite (c.a2))
        return {};

    return c;
}

/** RBJ peaking EQ. */
inline BiquadCoeffs makePeak (double sampleRate, double freqHz, double q, float gainDb) noexcept
{
    const auto A = std::pow (10.0, (double) gainDb / 40.0);
    const auto w0 = safeOmega (freqHz, sampleRate);
    const auto cosW0 = std::cos (w0);
    const auto alpha = std::sin (w0) / (2.0 * juce::jmax (0.05, q));

    return normalise (1.0 + alpha * A, -2.0 * cosW0, 1.0 - alpha * A,
                      1.0 + alpha / A, -2.0 * cosW0, 1.0 - alpha / A);
}

/** RBJ low shelf (S = 1). */
inline BiquadCoeffs makeLowShelf (double sampleRate, double freqHz, float gainDb) noexcept
{
    const auto A = std::pow (10.0, (double) gainDb / 40.0);
    const auto w0 = safeOmega (freqHz, sampleRate);
    const auto cosW0 = std::cos (w0);
    const auto alpha = std::sin (w0) * 0.5 * juce::MathConstants<double>::sqrt2;
    const auto twoSqrtAAlpha = 2.0 * std::sqrt (A) * alpha;

    return normalise (A * ((A + 1.0) - (A - 1.0) * cosW0 + twoSqrtAAlpha),
                      2.0 * A * ((A - 1.0) - (A + 1.0) * cosW0),
                      A * ((A + 1.0) - (A - 1.0) * cosW0 - twoSqrtAAlpha),
                      (A + 1.0) + (A - 1.0) * cosW0 + twoSqrtAAlpha,
                      -2.0 * ((A - 1.0) + (A + 1.0) * cosW0),
                      (A + 1.0) + (A - 1.0) * cosW0 - twoSqrtAAlpha);
}

/** RBJ high shelf (S = 1). */
inline BiquadCoeffs makeHighShelf (double sampleRate, double freqHz, float gainDb) noexcept
{
    const auto A = std::pow (10.0, (double) gainDb / 40.0);
    const auto w0 = safeOmega (freqHz, sampleRate);
    const auto cosW0 = std::cos (w0);
    const auto alpha = std::sin (w0) * 0.5 * juce::MathConstants<double>::sqrt2;
    const auto twoSqrtAAlpha = 2.0 * std::sqrt (A) * alpha;

    return normalise (A * ((A + 1.0) + (A - 1.0) * cosW0 + twoSqrtAAlpha),
                      -2.0 * A * ((A - 1.0) + (A + 1.0) * cosW0),
                      A * ((A + 1.0) + (A - 1.0) * cosW0 - twoSqrtAAlpha),
                      (A + 1.0) - (A - 1.0) * cosW0 + twoSqrtAAlpha,
                      2.0 * ((A - 1.0) - (A + 1.0) * cosW0),
                      (A + 1.0) - (A - 1.0) * cosW0 - twoSqrtAAlpha);
}

inline BiquadCoeffs makeLowPass (double sampleRate, double freqHz, double q) noexcept
{
    const auto w0 = safeOmega (freqHz, sampleRate);
    const auto cosW0 = std::cos (w0);
    const auto alpha = std::sin (w0) / (2.0 * juce::jmax (0.05, q));

    return normalise ((1.0 - cosW0) * 0.5, 1.0 - cosW0, (1.0 - cosW0) * 0.5,
                      1.0 + alpha, -2.0 * cosW0, 1.0 - alpha);
}

inline BiquadCoeffs makeHighPass (double sampleRate, double freqHz, double q) noexcept
{
    const auto w0 = safeOmega (freqHz, sampleRate);
    const auto cosW0 = std::cos (w0);
    const auto alpha = std::sin (w0) / (2.0 * juce::jmax (0.05, q));

    return normalise ((1.0 + cosW0) * 0.5, -(1.0 + cosW0), (1.0 + cosW0) * 0.5,
                      1.0 + alpha, -2.0 * cosW0, 1.0 - alpha);
}
} // namespace biquad

/**
 * One-pole smoother for a scalar parameter, stepped once per sample on the
 * audio thread. Used to keep REST-driven knob changes from zippering.
 */
class SmoothedParam
{
public:
    void reset (double sampleRate, double timeConstantSeconds, float initialValue) noexcept
    {
        const auto samples = juce::jmax (1.0, sampleRate * timeConstantSeconds);
        alpha = (float) (1.0 - std::exp (-1.0 / samples));
        current = initialValue;
    }

    void snapTo (float value) noexcept { current = value; }

    inline float next (float target) noexcept
    {
        current += (target - current) * alpha;

        if (! std::isfinite (current))
            current = target;

        return current;
    }

    float getCurrent() const noexcept { return current; }

private:
    float alpha = 1.0f;
    float current = 0.0f;
};
} // namespace milodikfx::dsp
