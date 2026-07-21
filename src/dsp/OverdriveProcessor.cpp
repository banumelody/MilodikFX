#include "dsp/OverdriveProcessor.h"

#include <cmath>

namespace milodikfx::dsp
{
OverdriveProcessor::OverdriveProcessor() = default;
OverdriveProcessor::~OverdriveProcessor() = default;

void OverdriveProcessor::prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels)
{
    const auto rate = sampleRate > 0.0 ? sampleRate : 44100.0;

    preparedChannels = juce::jmax (1, numChannels);
    preparedBlockSize = juce::jmax (1, samplesPerBlock);

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) preparedChannels,
        (size_t) kOversampleFactorLog2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true,
        false);

    oversampler->initProcessing ((size_t) preparedBlockSize);
    oversampler->reset();

    // The drive smoother is stepped in the oversampled domain, the level
    // smoother in the base domain, so they get different rates.
    smoothedDrive.reset (rate * kOversampleRatio, 0.02, driveAmount.load (std::memory_order_relaxed));
    smoothedLevel.reset (rate, 0.02, levelLinear.load (std::memory_order_relaxed));

    prepared = true;
}

float OverdriveProcessor::softClip (float x) noexcept
{
    if (std::abs (x) <= 1.0f)
        return x - (x * x * x) / 3.0f;

    return x > 0.0f ? (2.0f / 3.0f) : (-2.0f / 3.0f);
}

void OverdriveProcessor::applyDrive (float* const* channels, int numChannels, int numSamples, float driveTarget) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        const auto amount = smoothedDrive.next (driveTarget);

        // Pre-gain curve: 1x..20x, quadratic so the low end of the knob is usable.
        const auto driveGain = 1.0f + (amount * amount * 19.0f);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto x = channels[ch][i];

            // Cubic soft clip, scaled so saturation lands at +/-1.0.
            const auto clipped = 1.5f * softClip (x * driveGain);

            channels[ch][i] = ((1.0f - amount) * x) + (amount * clipped);
        }
    }
}

void OverdriveProcessor::applyLevel (float* const* channels, int numChannels, int numSamples, float levelTarget) noexcept
{
    if (std::abs (smoothedLevel.getCurrent() - levelTarget) < 1.0e-6f)
    {
        if (levelTarget == 1.0f)
            return;

        for (int ch = 0; ch < numChannels; ++ch)
            juce::FloatVectorOperations::multiply (channels[ch], levelTarget, numSamples);

        return;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const auto g = smoothedLevel.next (levelTarget);

        for (int ch = 0; ch < numChannels; ++ch)
            channels[ch][i] *= g;
    }
}

void OverdriveProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (! prepared)
        return;

    if (! enabled.load (std::memory_order_relaxed))
    {
        smoothedDrive.snapTo (driveAmount.load (std::memory_order_relaxed));
        smoothedLevel.snapTo (levelLinear.load (std::memory_order_relaxed));
        return;
    }

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    const auto driveTarget = driveAmount.load (std::memory_order_relaxed);
    const auto levelTarget = levelLinear.load (std::memory_order_relaxed);

    auto* const* channels = buffer.getArrayOfWritePointers();

    // No drive means no nonlinearity, so skip the oversampler entirely.
    const auto driveIsIdle = driveTarget <= 0.0f && smoothedDrive.getCurrent() <= 1.0e-6f;

    const auto canOversample = ! driveIsIdle
                               && oversamplingEnabled.load (std::memory_order_relaxed)
                               && oversampler != nullptr
                               && numChannels == preparedChannels
                               && numSamples <= preparedBlockSize;

    if (driveIsIdle)
    {
        smoothedDrive.snapTo (driveTarget);
    }
    else if (canOversample)
    {
        juce::dsp::AudioBlock<float> block (channels, (size_t) numChannels, (size_t) numSamples);
        auto upBlock = oversampler->processSamplesUp (juce::dsp::AudioBlock<const float> (block));

        const auto upSamples = (int) upBlock.getNumSamples();
        const auto upChannels = (int) upBlock.getNumChannels();

        // AudioBlock does not expose an array-of-pointers, so drive each channel
        // through a small stack array of the channel pointers it does expose.
        float* upPointers[32];
        const auto usableChannels = juce::jmin (upChannels, (int) std::size (upPointers));

        for (int ch = 0; ch < usableChannels; ++ch)
            upPointers[ch] = upBlock.getChannelPointer ((size_t) ch);

        applyDrive (upPointers, usableChannels, upSamples, driveTarget);

        oversampler->processSamplesDown (block);
    }
    else
    {
        applyDrive (channels, numChannels, numSamples, driveTarget);
    }

    applyLevel (channels, numChannels, numSamples, levelTarget);
}

void OverdriveProcessor::reset()
{
    if (oversampler != nullptr)
        oversampler->reset();

    smoothedDrive.snapTo (driveAmount.load (std::memory_order_relaxed));
    smoothedLevel.snapTo (levelLinear.load (std::memory_order_relaxed));
}

void OverdriveProcessor::setDrivePercent (float percent) noexcept
{
    const auto clamped = juce::jlimit (0.0f, 100.0f, percent);
    drivePercent.store (clamped, std::memory_order_relaxed);
    driveAmount.store (clamped / 100.0f, std::memory_order_relaxed);
}

float OverdriveProcessor::getDrivePercent() const noexcept
{
    return drivePercent.load (std::memory_order_relaxed);
}

void OverdriveProcessor::setLevelPercent (float percent) noexcept
{
    const auto clamped = juce::jlimit (0.0f, 100.0f, percent);
    levelPercent.store (clamped, std::memory_order_relaxed);
    levelLinear.store (clamped / 100.0f, std::memory_order_relaxed);
}

float OverdriveProcessor::getLevelPercent() const noexcept
{
    return levelPercent.load (std::memory_order_relaxed);
}

void OverdriveProcessor::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable, std::memory_order_relaxed);
}

bool OverdriveProcessor::isEnabled() const noexcept
{
    return enabled.load (std::memory_order_relaxed);
}

void OverdriveProcessor::setOversamplingEnabled (bool shouldEnable) noexcept
{
    oversamplingEnabled.store (shouldEnable, std::memory_order_relaxed);
}

bool OverdriveProcessor::isOversamplingEnabled() const noexcept
{
    return oversamplingEnabled.load (std::memory_order_relaxed);
}

float OverdriveProcessor::getLatencySamples() const noexcept
{
    if (oversampler == nullptr || ! oversamplingEnabled.load (std::memory_order_relaxed))
        return 0.0f;

    return oversampler->getLatencyInSamples();
}
} // namespace milodikfx::dsp
