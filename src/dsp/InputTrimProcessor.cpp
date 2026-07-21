#include "dsp/InputTrimProcessor.h"

#include <cmath>

namespace milodikfx::dsp
{
namespace
{
/** Fast enough to feel immediate, slow enough not to zipper on a drag. */
constexpr double kSmoothingSeconds = 0.02;

/** Below this the smoother counts as settled on its target. */
constexpr float kSettledEpsilon = 1.0e-6f;
} // namespace

void InputTrimProcessor::prepareToPlay (double sampleRate, int, int)
{
    smoothedGain.reset (sampleRate > 0.0 ? sampleRate : 44100.0, kSmoothingSeconds,
                        gainLinear.load (std::memory_order_relaxed));
    prepared = true;
}

void InputTrimProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (! prepared)
        return;

    const auto target = gainLinear.load (std::memory_order_relaxed);
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    if (std::abs (smoothedGain.getCurrent() - target) < kSettledEpsilon)
    {
        // Settled at unity: leave the buffer completely alone. The default has
        // to be a true passthrough, not a multiply by 1.0f that a null test
        // could catch drifting.
        if (target == 1.0f)
            return;

        buffer.applyGain (target);
        return;
    }

    auto* const* channels = buffer.getArrayOfWritePointers();

    for (int i = 0; i < numSamples; ++i)
    {
        const auto g = smoothedGain.next (target);

        for (int ch = 0; ch < numChannels; ++ch)
            channels[ch][i] *= g;
    }
}

void InputTrimProcessor::reset()
{
    smoothedGain.snapTo (gainLinear.load (std::memory_order_relaxed));
}

void InputTrimProcessor::setGainDb (float db) noexcept
{
    if (! std::isfinite (db))
        return;

    const auto clamped = juce::jlimit (kMinDb, kMaxDb, db);

    gainDb.store (clamped, std::memory_order_relaxed);

    // Exactly 1.0f at 0 dB, so the passthrough test above is reachable rather
    // than depending on decibelsToGain rounding to unity.
    gainLinear.store (clamped == 0.0f ? 1.0f : juce::Decibels::decibelsToGain (clamped),
                      std::memory_order_relaxed);
}

float InputTrimProcessor::getGainDb() const noexcept
{
    return gainDb.load (std::memory_order_relaxed);
}
} // namespace milodikfx::dsp
