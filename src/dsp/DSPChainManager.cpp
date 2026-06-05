#include "dsp/DSPChainManager.h"

namespace milodikfx::dsp
{
void DSPChainManager::prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels)
{
    currentSampleRate = sampleRate;
    currentSamplesPerBlock = samplesPerBlock;
    currentNumChannels = numChannels;
    prepared = true;

    for (const auto& processor : processors)
        if (processor != nullptr)
            processor->prepareToPlay (sampleRate, samplesPerBlock, numChannels);
}

void DSPChainManager::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (bypassed.load (std::memory_order_relaxed))
        return;

    for (const auto& processor : processors)
        if (processor != nullptr)
            processor->processBlock (buffer);
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
