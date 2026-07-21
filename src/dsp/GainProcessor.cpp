#include "dsp/GainProcessor.h"

namespace milodikfx::dsp
{
void GainProcessor::prepareToPlay (double sampleRate, int, int)
{
    smoothedGain.reset (sampleRate > 0.0 ? sampleRate : 44100.0, 0.02,
                        gainLinear.load (std::memory_order_relaxed));
    prepared = true;
}

void GainProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (! prepared)
        return;

    if (! enabled.load (std::memory_order_relaxed))
    {
        // Keep the smoother tracking so re-enabling does not jump.
        smoothedGain.snapTo (gainLinear.load (std::memory_order_relaxed));
        return;
    }

    const auto target = gainLinear.load (std::memory_order_relaxed);
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    // Already settled: the common case, so take the cheap path.
    if (std::abs (smoothedGain.getCurrent() - target) < 1.0e-6f)
    {
        if (target != 1.0f)
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

void GainProcessor::reset()
{
    smoothedGain.snapTo (gainLinear.load (std::memory_order_relaxed));
}

void GainProcessor::setGainDb (float db) noexcept
{
    const auto clamped = juce::jlimit (0.0f, 24.0f, db);
    gainDb.store (clamped, std::memory_order_relaxed);
    gainLinear.store (juce::Decibels::decibelsToGain (clamped), std::memory_order_relaxed);
}

float GainProcessor::getGainDb() const noexcept
{
    return gainDb.load (std::memory_order_relaxed);
}

void GainProcessor::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable, std::memory_order_relaxed);
}

bool GainProcessor::isEnabled() const noexcept
{
    return enabled.load (std::memory_order_relaxed);
}
} // namespace milodikfx::dsp
