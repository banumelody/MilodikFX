#include "CompressorProcessor.h"
#include <cmath>

namespace milodikfx::dsp {

void CompressorProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock, int numChannels)
{
    juce::ignoreUnused (samplesPerBlock, numChannels);
    sampleRate = sampleRateIn > 0.0 ? sampleRateIn : 44100.0;
    timingDirty.store (true, std::memory_order_relaxed);
    smoothedMix.reset (sampleRate, 0.02, mixPercent.load (std::memory_order_relaxed) / 100.0f);
    reset();
    updateCoefficientsIfNeeded();
}

void CompressorProcessor::updateCoefficientsIfNeeded() noexcept
{
    const auto a = attackMs.load (std::memory_order_relaxed);
    const auto r = releaseMs.load (std::memory_order_relaxed);

    if (! timingDirty.load (std::memory_order_relaxed) && a == lastAttackMs && r == lastReleaseMs)
        return;

    lastAttackMs = a;
    lastReleaseMs = r;
    timingDirty.store (false, std::memory_order_relaxed);

    // One-pole time constant: reach ~63% of the target within the stated time.
    const auto attackSamples = juce::jmax (1.0, (double) a * sampleRate / 1000.0);
    const auto releaseSamples = juce::jmax (1.0, (double) r * sampleRate / 1000.0);

    alphaAttack = (float) (1.0 - std::exp (-1.0 / attackSamples));
    alphaRelease = (float) (1.0 - std::exp (-1.0 / releaseSamples));
}

void CompressorProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (! enabled.load (std::memory_order_relaxed))
    {
        gainReductionDb.store (0.0f, std::memory_order_relaxed);
        return;
    }

    updateCoefficientsIfNeeded();

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    if (numChannels <= 0 || numSamples <= 0)
        return;

    const auto inGain = juce::Decibels::decibelsToGain (inputGainDb.load (std::memory_order_relaxed));
    const auto threshold = thresholdDb.load (std::memory_order_relaxed);

    // slope is the fraction of every dB above the threshold that gets removed.
    // ratio is clamped >= 1 by the setter, so slope stays in [0, 1) and the
    // gain computer can never divide by zero or produce a positive gain.
    const auto ratioValue = juce::jmax (1.0f, ratio.load (std::memory_order_relaxed));
    const auto slope = 1.0f - (1.0f / ratioValue);

    // Fixed makeup derived from the static curve: how much a signal sitting at
    // the threshold loses. Independent of the envelope, so it can never cancel
    // the compression out the way the old reciprocal-of-envelope makeup did.
    auto makeupDb = 0.0f;
    if (autoMakeupGain.load (std::memory_order_relaxed))
        makeupDb = juce::jlimit (0.0f, 24.0f, -threshold * slope * 0.5f);

    const auto mixTarget = juce::jlimit (0.0f, 1.0f, mixPercent.load (std::memory_order_relaxed) / 100.0f);

    auto* const* channels = buffer.getArrayOfWritePointers();
    auto minEnvelopeThisBlock = 0.0f;

    const auto activeChannels = juce::jmin (numChannels, kMaxChannels);

    // Untouched copy of the current sample, kept so the compressed result can be
    // blended back against it for parallel ("New York") compression. Small enough
    // to live on the stack, so nothing is allocated here.
    float dry[kMaxChannels] {};

    for (int i = 0; i < numSamples; ++i)
    {
        float peak = 0.0f;

        for (int ch = 0; ch < activeChannels; ++ch)
        {
            dry[ch] = channels[ch][i];

            const auto s = dry[ch] * inGain;
            channels[ch][i] = s;
            peak = juce::jmax (peak, std::abs (s));
        }

        const auto sampleDb = juce::Decibels::gainToDecibels (peak, kDetectorFloorDb);
        const auto overDb = sampleDb - threshold;
        const auto targetDb = overDb > 0.0f ? -(overDb * slope) : 0.0f;

        // Attack when we need to pull the gain down further, release on the way back up.
        const auto alpha = targetDb < envelopeDb ? alphaAttack : alphaRelease;
        envelopeDb += (targetDb - envelopeDb) * alpha;

        if (! std::isfinite (envelopeDb))
            envelopeDb = 0.0f;

        envelopeDb = juce::jlimit (-60.0f, 0.0f, envelopeDb);
        minEnvelopeThisBlock = juce::jmin (minEnvelopeThisBlock, envelopeDb);

        const auto gain = juce::Decibels::decibelsToGain (envelopeDb + makeupDb);
        const auto mix = smoothedMix.next (mixTarget);

        for (int ch = 0; ch < activeChannels; ++ch)
            channels[ch][i] = dry[ch] * (1.0f - mix) + channels[ch][i] * gain * mix;
    }

    gainReductionDb.store (minEnvelopeThisBlock, std::memory_order_relaxed);
}

void CompressorProcessor::reset()
{
    envelopeDb = 0.0f;
    gainReductionDb.store (0.0f, std::memory_order_relaxed);
    smoothedMix.snapTo (mixPercent.load (std::memory_order_relaxed) / 100.0f);
}

void CompressorProcessor::setMixPercent (float percent) noexcept
{
    mixPercent.store (juce::jlimit (0.0f, 100.0f, percent), std::memory_order_relaxed);
}

float CompressorProcessor::getMixPercent() const noexcept
{
    return mixPercent.load (std::memory_order_relaxed);
}

void CompressorProcessor::setInputGainDb (float db) noexcept
{
    inputGainDb.store (juce::jlimit (-24.0f, 24.0f, db), std::memory_order_relaxed);
}

void CompressorProcessor::setThresholdDb (float db) noexcept
{
    thresholdDb.store (juce::jlimit (-60.0f, 0.0f, db), std::memory_order_relaxed);
}

void CompressorProcessor::setRatio (float ratioValue) noexcept
{
    ratio.store (juce::jlimit (1.0f, 20.0f, ratioValue), std::memory_order_relaxed);
}

void CompressorProcessor::setAttackMs (float ms) noexcept
{
    attackMs.store (juce::jlimit (0.1f, 200.0f, ms), std::memory_order_relaxed);
    timingDirty.store (true, std::memory_order_relaxed);
}

void CompressorProcessor::setReleaseMs (float ms) noexcept
{
    releaseMs.store (juce::jlimit (5.0f, 2000.0f, ms), std::memory_order_relaxed);
    timingDirty.store (true, std::memory_order_relaxed);
}

void CompressorProcessor::setAutoMakeupGain (bool shouldEnable) noexcept
{
    autoMakeupGain.store (shouldEnable, std::memory_order_relaxed);
}

void CompressorProcessor::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable, std::memory_order_relaxed);
}

float CompressorProcessor::getInputGainDb() const noexcept
{
    return inputGainDb.load (std::memory_order_relaxed);
}

float CompressorProcessor::getThresholdDb() const noexcept
{
    return thresholdDb.load (std::memory_order_relaxed);
}

float CompressorProcessor::getRatio() const noexcept
{
    return ratio.load (std::memory_order_relaxed);
}

float CompressorProcessor::getAttackMs() const noexcept
{
    return attackMs.load (std::memory_order_relaxed);
}

float CompressorProcessor::getReleaseMs() const noexcept
{
    return releaseMs.load (std::memory_order_relaxed);
}

bool CompressorProcessor::getAutoMakeupGain() const noexcept
{
    return autoMakeupGain.load (std::memory_order_relaxed);
}

bool CompressorProcessor::isEnabled() const noexcept
{
    return enabled.load (std::memory_order_relaxed);
}

float CompressorProcessor::getGainReductionDb() const noexcept
{
    return gainReductionDb.load (std::memory_order_relaxed);
}

} // namespace milodikfx::dsp
