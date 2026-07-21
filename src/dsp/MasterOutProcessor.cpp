#include "dsp/MasterOutProcessor.h"

#include <cmath>

namespace milodikfx::dsp
{
void MasterOutProcessor::updateCoefficients() noexcept
{
    alphaAttack = (float) (1.0 - std::exp (-1.0 / juce::jmax (1.0, (double) kAttackMs * sampleRate / 1000.0)));
    alphaRelease = (float) (1.0 - std::exp (-1.0 / juce::jmax (1.0, (double) kReleaseMs * sampleRate / 1000.0)));
}

void MasterOutProcessor::prepareToPlay (double sampleRateIn, int, int)
{
    sampleRate = sampleRateIn > 0.0 ? sampleRateIn : 44100.0;

    updateCoefficients();
    smoothedVolume.reset (sampleRate, 0.02, volumeLinear.load (std::memory_order_relaxed));
    reset();

    prepared = true;
}

void MasterOutProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (! prepared)
        return;

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    const auto targetVolume = muted.load (std::memory_order_relaxed)
                                  ? 0.0f
                                  : volumeLinear.load (std::memory_order_relaxed);

    const auto limiting = limiterEnabled.load (std::memory_order_relaxed);
    const auto ceiling = ceilingDb.load (std::memory_order_relaxed);
    const auto ceilingLinear = juce::Decibels::decibelsToGain (ceiling);

    auto* const* channels = buffer.getArrayOfWritePointers();
    auto minEnvelopeThisBlock = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto volume = smoothedVolume.next (targetVolume);

        float peak = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto s = channels[ch][i] * volume;

            // A NaN arriving from upstream stops here rather than reaching the driver.
            if (! std::isfinite (s))
                s = 0.0f;

            channels[ch][i] = s;
            peak = juce::jmax (peak, std::abs (s));
        }

        auto limiterGain = 1.0f;

        if (limiting)
        {
            const auto peakDb = juce::Decibels::gainToDecibels (peak, kDetectorFloorDb);
            const auto targetDb = juce::jmin (0.0f, ceiling - peakDb);

            const auto alpha = targetDb < envelopeDb ? alphaAttack : alphaRelease;
            envelopeDb += (targetDb - envelopeDb) * alpha;

            if (! std::isfinite (envelopeDb))
                envelopeDb = 0.0f;

            envelopeDb = juce::jlimit (-60.0f, 0.0f, envelopeDb);
            minEnvelopeThisBlock = juce::jmin (minEnvelopeThisBlock, envelopeDb);

            limiterGain = juce::Decibels::decibelsToGain (envelopeDb);
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto s = channels[ch][i] * limiterGain;

            // Last line of defence: the envelope follower always lags, so clamp
            // whatever slipped through before it reaches the interface.
            channels[ch][i] = juce::jlimit (-ceilingLinear, ceilingLinear, s);
        }
    }

    limiterReductionDb.store (minEnvelopeThisBlock, std::memory_order_relaxed);
}

void MasterOutProcessor::reset()
{
    envelopeDb = 0.0f;
    limiterReductionDb.store (0.0f, std::memory_order_relaxed);
    smoothedVolume.snapTo (muted.load (std::memory_order_relaxed)
                               ? 0.0f
                               : volumeLinear.load (std::memory_order_relaxed));
}

void MasterOutProcessor::setVolumeDb (float db) noexcept
{
    const auto clamped = juce::jlimit (kMinVolumeDb, kMaxVolumeDb, db);
    volumeDb.store (clamped, std::memory_order_relaxed);

    // The bottom of the range is a true mute rather than -60 dB of bleed.
    volumeLinear.store (clamped <= kMinVolumeDb ? 0.0f : juce::Decibels::decibelsToGain (clamped),
                        std::memory_order_relaxed);
}

float MasterOutProcessor::getVolumeDb() const noexcept
{
    return volumeDb.load (std::memory_order_relaxed);
}

void MasterOutProcessor::setMuted (bool shouldMute) noexcept
{
    muted.store (shouldMute, std::memory_order_relaxed);
}

bool MasterOutProcessor::isMuted() const noexcept
{
    return muted.load (std::memory_order_relaxed);
}

void MasterOutProcessor::setLimiterEnabled (bool shouldEnable) noexcept
{
    limiterEnabled.store (shouldEnable, std::memory_order_relaxed);
}

bool MasterOutProcessor::isLimiterEnabled() const noexcept
{
    return limiterEnabled.load (std::memory_order_relaxed);
}

void MasterOutProcessor::setCeilingDb (float db) noexcept
{
    ceilingDb.store (juce::jlimit (-24.0f, 0.0f, db), std::memory_order_relaxed);
}

float MasterOutProcessor::getCeilingDb() const noexcept
{
    return ceilingDb.load (std::memory_order_relaxed);
}

float MasterOutProcessor::getLimiterReductionDb() const noexcept
{
    return limiterReductionDb.load (std::memory_order_relaxed);
}
} // namespace milodikfx::dsp
