#include "dsp/NoiseGateProcessor.h"

#include <cmath>

namespace milodikfx::dsp
{
void NoiseGateProcessor::prepareToPlay (double sampleRateIn, int, int)
{
    sampleRate = sampleRateIn > 0.0 ? sampleRateIn : 44100.0;
    timingDirty.store (true, std::memory_order_relaxed);
    reset();
    updateCoefficientsIfNeeded();
    prepared = true;
}

void NoiseGateProcessor::updateCoefficientsIfNeeded() noexcept
{
    const auto a = attackMs.load (std::memory_order_relaxed);
    const auto r = releaseMs.load (std::memory_order_relaxed);
    const auto h = holdMs.load (std::memory_order_relaxed);

    if (! timingDirty.load (std::memory_order_relaxed) && a == lastAttackMs && r == lastReleaseMs && h == lastHoldMs)
        return;

    lastAttackMs = a;
    lastReleaseMs = r;
    lastHoldMs = h;
    timingDirty.store (false, std::memory_order_relaxed);

    alphaAttack = (float) (1.0 - std::exp (-1.0 / juce::jmax (1.0, (double) a * sampleRate / 1000.0)));
    alphaRelease = (float) (1.0 - std::exp (-1.0 / juce::jmax (1.0, (double) r * sampleRate / 1000.0)));
    holdSamples = (int) juce::jmax (0.0, (double) h * sampleRate / 1000.0);
}

void NoiseGateProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (! prepared)
        return;

    if (! enabled.load (std::memory_order_relaxed))
    {
        gain = 1.0f;
        holdCounter = 0;
        currentGain.store (1.0f, std::memory_order_relaxed);
        return;
    }

    updateCoefficientsIfNeeded();

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    const auto openThreshold = juce::Decibels::decibelsToGain (thresholdDb.load (std::memory_order_relaxed));

    // Close a little below the open point so a signal hovering at the threshold
    // does not rattle the gate open and shut.
    const auto closeThreshold = openThreshold * 0.7f;

    auto* const* channels = buffer.getArrayOfWritePointers();

    for (int i = 0; i < numSamples; ++i)
    {
        float peak = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
            peak = juce::jmax (peak, std::abs (channels[ch][i]));

        if (peak > openThreshold)
            holdCounter = holdSamples;
        else if (peak < closeThreshold && holdCounter > 0)
            --holdCounter;

        const auto target = (peak > openThreshold || holdCounter > 0) ? 1.0f : 0.0f;
        const auto alpha = target > gain ? alphaAttack : alphaRelease;

        gain += (target - gain) * alpha;

        if (! std::isfinite (gain))
            gain = 1.0f;

        gain = juce::jlimit (0.0f, 1.0f, gain);

        for (int ch = 0; ch < numChannels; ++ch)
            channels[ch][i] *= gain;
    }

    currentGain.store (gain, std::memory_order_relaxed);
}

void NoiseGateProcessor::reset()
{
    gain = 1.0f;
    holdCounter = 0;
    currentGain.store (1.0f, std::memory_order_relaxed);
}

void NoiseGateProcessor::setThresholdDb (float db) noexcept
{
    thresholdDb.store (juce::jlimit (-90.0f, 0.0f, db), std::memory_order_relaxed);
}

float NoiseGateProcessor::getThresholdDb() const noexcept
{
    return thresholdDb.load (std::memory_order_relaxed);
}

void NoiseGateProcessor::setAttackMs (float ms) noexcept
{
    attackMs.store (juce::jlimit (0.1f, 50.0f, ms), std::memory_order_relaxed);
    timingDirty.store (true, std::memory_order_relaxed);
}

float NoiseGateProcessor::getAttackMs() const noexcept
{
    return attackMs.load (std::memory_order_relaxed);
}

void NoiseGateProcessor::setHoldMs (float ms) noexcept
{
    holdMs.store (juce::jlimit (0.0f, 500.0f, ms), std::memory_order_relaxed);
    timingDirty.store (true, std::memory_order_relaxed);
}

float NoiseGateProcessor::getHoldMs() const noexcept
{
    return holdMs.load (std::memory_order_relaxed);
}

void NoiseGateProcessor::setReleaseMs (float ms) noexcept
{
    releaseMs.store (juce::jlimit (5.0f, 1000.0f, ms), std::memory_order_relaxed);
    timingDirty.store (true, std::memory_order_relaxed);
}

float NoiseGateProcessor::getReleaseMs() const noexcept
{
    return releaseMs.load (std::memory_order_relaxed);
}

void NoiseGateProcessor::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable, std::memory_order_relaxed);
}

bool NoiseGateProcessor::isEnabled() const noexcept
{
    return enabled.load (std::memory_order_relaxed);
}

float NoiseGateProcessor::getCurrentGain() const noexcept
{
    return currentGain.load (std::memory_order_relaxed);
}
} // namespace milodikfx::dsp
