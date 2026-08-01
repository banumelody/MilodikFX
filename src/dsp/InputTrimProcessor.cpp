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

/** Exactly 1.0f at 0 dB, so a true passthrough stays reachable. */
float linearFor (float db) noexcept
{
    return db == 0.0f ? 1.0f : juce::Decibels::decibelsToGain (db);
}
} // namespace

void InputTrimProcessor::prepareToPlay (double sampleRate, int, int)
{
    const auto rate = sampleRate > 0.0 ? sampleRate : 44100.0;

    for (int ch = 0; ch < kMaxChannels; ++ch)
        smoothedGain[(size_t) ch].reset (rate, kSmoothingSeconds,
                                         gainLinear[(size_t) ch].load (std::memory_order_relaxed));

    prepared = true;
}

void InputTrimProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (! prepared)
        return;

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin (buffer.getNumChannels(), kMaxChannels);

    if (numSamples <= 0 || numChannels <= 0)
        return;

    auto* const* channels = buffer.getArrayOfWritePointers();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const auto target = gainLinear[(size_t) ch].load (std::memory_order_relaxed);
        auto& smoother = smoothedGain[(size_t) ch];

        if (std::abs (smoother.getCurrent() - target) < kSettledEpsilon)
        {
            // Settled at unity: leave the channel completely alone. The default
            // has to be a true passthrough, not a multiply by 1.0f that a null
            // test could catch drifting.
            if (target == 1.0f)
                continue;

            juce::FloatVectorOperations::multiply (channels[ch], target, numSamples);
            continue;
        }

        for (int i = 0; i < numSamples; ++i)
            channels[ch][i] *= smoother.next (target);
    }
}

void InputTrimProcessor::reset()
{
    for (int ch = 0; ch < kMaxChannels; ++ch)
        smoothedGain[(size_t) ch].snapTo (gainLinear[(size_t) ch].load (std::memory_order_relaxed));
}

void InputTrimProcessor::refreshTargets() noexcept
{
    const auto left = gainDb.load (std::memory_order_relaxed);

    // Linked means the right channel *is* the left one, not a copy that could
    // fall behind: it reads the same figure rather than being written to.
    const auto right = linked.load (std::memory_order_relaxed)
                           ? left
                           : gainDbRight.load (std::memory_order_relaxed);

    gainLinear[0].store (linearFor (left), std::memory_order_relaxed);
    gainLinear[1].store (linearFor (right), std::memory_order_relaxed);
}

void InputTrimProcessor::setGainDb (float db) noexcept
{
    if (! std::isfinite (db))
        return;

    gainDb.store (juce::jlimit (kMinDb, kMaxDb, db), std::memory_order_relaxed);
    refreshTargets();
}

float InputTrimProcessor::getGainDb() const noexcept
{
    return gainDb.load (std::memory_order_relaxed);
}

void InputTrimProcessor::setGainDbR (float db) noexcept
{
    if (! std::isfinite (db))
        return;

    // Stored even while linked, so unlinking restores what was dialled rather
    // than snapping the right channel to whatever the left happens to be.
    gainDbRight.store (juce::jlimit (kMinDb, kMaxDb, db), std::memory_order_relaxed);
    refreshTargets();
}

float InputTrimProcessor::getGainDbR() const noexcept
{
    return gainDbRight.load (std::memory_order_relaxed);
}

void InputTrimProcessor::setLinked (bool shouldLink) noexcept
{
    linked.store (shouldLink, std::memory_order_relaxed);
    refreshTargets();
}

float InputTrimProcessor::getEffectiveGainDb (int channel) const noexcept
{
    if (channel <= 0 || linked.load (std::memory_order_relaxed))
        return gainDb.load (std::memory_order_relaxed);

    return gainDbRight.load (std::memory_order_relaxed);
}
} // namespace milodikfx::dsp
