#include "dsp/GainProcessor.h"

namespace milodikfx::dsp
{
void GainProcessor::prepareToPlay (double, int, int)
{
    prepared = true;
}

void GainProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (! prepared)
        return;

    if (! enabled.load (std::memory_order_relaxed))
        return;

    buffer.applyGain (gainLinear.load (std::memory_order_relaxed));
}

void GainProcessor::reset()
{
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
