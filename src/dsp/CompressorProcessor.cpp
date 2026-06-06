#include "CompressorProcessor.h"
#include <cmath>

namespace milodikfx::dsp {

void CompressorProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock, int numChannels)
{
    juce::ignoreUnused (samplesPerBlock, numChannels);
    sampleRate = sampleRateIn;
    updateCoefficients();
}

void CompressorProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (!enabled.load())
        return;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* samples = buffer.getWritePointer (ch);
        const int numSamples = buffer.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
        {
            // Apply input gain
            float sample = samples[i] * std::pow (10.0f, inputGainDb.load() / 20.0f);

            // Measure RMS in dB
            float sampleDb = 20.0f * std::log10 (std::abs (sample) + 1e-6f);

            // Calculate gain reduction
            float gainReduction = 1.0f;
            if (sampleDb > thresholdDb.load())
            {
                float excess = sampleDb - thresholdDb.load();
                float compressionRatio = ratio.load();
                gainReduction = 1.0f / (1.0f + (compressionRatio - 1.0f) * (excess / thresholdDb.load()));
            }

            // Apply attack/release envelope
            float targetDb = 20.0f * std::log10 (gainReduction + 1e-6f);
            if (targetDb < envelopeDb)
                envelopeDb += (targetDb - envelopeDb) * alphaAttack;
            else
                envelopeDb += (targetDb - envelopeDb) * alphaRelease;

            // Apply makeup gain if enabled
            float makeupGain = 1.0f;
            if (autoMakeupGain.load())
                makeupGain = std::pow (10.0f, -envelopeDb / 20.0f);

            samples[i] = sample * std::pow (10.0f, envelopeDb / 20.0f) * makeupGain;
        }
    }
}

void CompressorProcessor::reset()
{
    envelopeDb = -100.0f;
}

void CompressorProcessor::setInputGainDb (float db) noexcept
{
    inputGainDb.store (db);
}

void CompressorProcessor::setThresholdDb (float db) noexcept
{
    thresholdDb.store (juce::jlimit (-60.0f, 0.0f, db));
}

void CompressorProcessor::setRatio (float ratioValue) noexcept
{
    ratio.store (juce::jlimit (1.0f, 16.0f, ratioValue));
}

void CompressorProcessor::setAttackMs (float ms) noexcept
{
    attackMs.store (juce::jlimit (0.1f, 100.0f, ms));
    updateCoefficients();
}

void CompressorProcessor::setReleaseMs (float ms) noexcept
{
    releaseMs.store (juce::jlimit (10.0f, 1000.0f, ms));
    updateCoefficients();
}

void CompressorProcessor::setAutoMakeupGain (bool enabled) noexcept
{
    autoMakeupGain.store (enabled);
}

void CompressorProcessor::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable);
}

float CompressorProcessor::getInputGainDb() const noexcept
{
    return inputGainDb.load();
}

float CompressorProcessor::getThresholdDb() const noexcept
{
    return thresholdDb.load();
}

float CompressorProcessor::getRatio() const noexcept
{
    return ratio.load();
}

float CompressorProcessor::getAttackMs() const noexcept
{
    return attackMs.load();
}

float CompressorProcessor::getReleaseMs() const noexcept
{
    return releaseMs.load();
}

bool CompressorProcessor::getAutoMakeupGain() const noexcept
{
    return autoMakeupGain.load();
}

bool CompressorProcessor::isEnabled() const noexcept
{
    return enabled.load();
}

void CompressorProcessor::updateCoefficients()
{
    // Simple 1-pole low-pass for envelope following
    alphaAttack = 2.0f / (attackMs.load() * (float)sampleRate / 1000.0f + 1.0f);
    alphaRelease = 2.0f / (releaseMs.load() * (float)sampleRate / 1000.0f + 1.0f);
}

} // namespace milodikfx::dsp
