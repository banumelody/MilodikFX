#include "dsp/DSPChainManager.h"

namespace milodikfx::dsp
{
DSPChainManager::DSPChainManager()
{
    // Allocated up front and never resized: prepareToPlay must not move memory
    // that a block already in flight on the audio thread is reading.
    dryCopy.setSize (kMaxChannels, kMaxBlockSize, false, true, false);
}

void DSPChainManager::prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels)
{
    currentSampleRate = sampleRate;
    currentSamplesPerBlock = samplesPerBlock;
    currentNumChannels = numChannels;
    prepared = true;

    const auto fadeSamples = juce::jmax (1.0, (sampleRate > 0.0 ? sampleRate : 44100.0) * kBypassFadeSeconds);
    fadeStep = (float) (1.0 / fadeSamples);

    wetGain = bypassed.load (std::memory_order_relaxed) ? 0.0f : 1.0f;

    for (const auto& processor : processors)
        if (processor != nullptr)
            processor->prepareToPlay (sampleRate, samplesPerBlock, numChannels);
}

void DSPChainManager::processBlock (juce::AudioBuffer<float>& buffer)
{
    const auto wantBypass = bypassed.load (std::memory_order_relaxed);
    const auto targetGain = wantBypass ? 0.0f : 1.0f;

    // Settled in bypass: the dry signal is already what the caller wants.
    if (wantBypass && wetGain <= 0.0f)
        return;

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin (buffer.getNumChannels(), kMaxChannels);
    const auto needsFade = wetGain != targetGain;

    // A hard switch clicks. Keep the dry signal so the two can be crossfaded,
    // but only pay for the copy while a fade is actually in progress.
    const auto canFade = needsFade
                         && numChannels > 0
                         && numSamples > 0
                         && numSamples <= dryCopy.getNumSamples();

    if (canFade)
        for (int ch = 0; ch < numChannels; ++ch)
            dryCopy.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    for (const auto& processor : processors)
        if (processor != nullptr)
            processor->processBlock (buffer);

    if (! needsFade)
        return;

    if (! canFade)
    {
        // Cannot crossfade this block, so land on the target rather than
        // leaving the gain stuck part-way.
        wetGain = targetGain;
        return;
    }

    auto* const* channels = buffer.getArrayOfWritePointers();
    auto gain = wetGain;

    for (int i = 0; i < numSamples; ++i)
    {
        gain += targetGain > gain ? fadeStep : -fadeStep;
        gain = juce::jlimit (0.0f, 1.0f, gain);

        for (int ch = 0; ch < numChannels; ++ch)
            channels[ch][i] = dryCopy.getSample (ch, i) * (1.0f - gain) + channels[ch][i] * gain;
    }

    wetGain = gain;
}

void DSPChainManager::reset()
{
    for (const auto& processor : processors)
        if (processor != nullptr)
            processor->reset();
}

AudioProcessorBase* DSPChainManager::addProcessor (std::unique_ptr<AudioProcessorBase> processor)
{
    if (processor == nullptr)
        return nullptr;

    if (prepared)
        processor->prepareToPlay (currentSampleRate, currentSamplesPerBlock, currentNumChannels);

    auto* raw = processor.get();
    processors.push_back (std::move (processor));
    return raw;
}

void DSPChainManager::clear()
{
    processors.clear();
}

void DSPChainManager::setBypassed (bool shouldBypass) noexcept
{
    bypassed.store (shouldBypass, std::memory_order_relaxed);
}

bool DSPChainManager::isBypassed() const noexcept
{
    return bypassed.load (std::memory_order_relaxed);
}

int DSPChainManager::getNumProcessors() const noexcept
{
    return static_cast<int> (processors.size());
}
} // namespace milodikfx::dsp
