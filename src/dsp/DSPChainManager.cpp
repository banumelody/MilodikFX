#include "dsp/DSPChainManager.h"

#include <array>

#include "dsp/MixerProcessor.h"
#include "dsp/SplitProcessor.h"

namespace milodikfx::dsp
{
DSPChainManager::DSPChainManager()
{
    // Allocated up front and never resized: prepareToPlay must not move memory
    // that a block already in flight on the audio thread is reading.
    dryCopy.setSize (kMaxChannels, kMaxBlockSize, false, true, false);

    // Path B, for the parallel section. Allocated here for the same reason
    // dryCopy is: prepareToPlay must never move memory a block in flight is
    // reading, and the audio thread must never allocate.
    pathB.setSize (kMaxChannels, kMaxBlockSize, false, true, false);
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

    for (const auto& processor : postProcessors)
        if (processor != nullptr)
            processor->prepareToPlay (sampleRate, samplesPerBlock, numChannels);
}

void DSPChainManager::processBlock (juce::AudioBuffer<float>& buffer)
{
    processChain (buffer);

    // Outside the bypass logic on purpose: these are mixed into the output, so
    // bypassing the effects must not silence them.
    for (const auto& processor : postProcessors)
        if (processor != nullptr)
            processor->processBlock (buffer);
}

void DSPChainManager::processChain (juce::AudioBuffer<float>& buffer)
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

    // One acquire load for the whole block: either the old order or the new one,
    // never a mix of the two, and no allocation on either side of the handover.
    const auto order = packedOrder.load (std::memory_order_acquire);
    const auto buses = busBMask.load (std::memory_order_acquire);
    const auto count = (int) processors.size();

    // The parallel section, if there is one. `running` turns on at the split
    // stage and off at the mixer; while it is on, each stage runs on whichever
    // buffer its bus bit names.
    const auto canSplit = splitStage != nullptr
                          && mixerStage != nullptr
                          && numChannels > 0
                          && numSamples > 0
                          && numSamples <= pathB.getNumSamples();

    juce::AudioBuffer<float> pathBView (pathB.getArrayOfWritePointers(), numChannels, numSamples);
    auto running = false;

    for (int position = 0; position < count; ++position)
    {
        const auto index = (int) ((order >> (position * 4)) & 0xFULL);

        if (index >= count || processors[(size_t) index] == nullptr)
            continue;

        auto* processor = processors[(size_t) index].get();

        if (processor == splitStage)
        {
            // Only a split that is switched on opens a second path; otherwise
            // the chain stays serial and byte-for-byte what it was before any of
            // this existed.
            if (canSplit && splitIsActive())
            {
                divideAtSplit (buffer, pathBView);
                running = true;
            }

            continue;
        }

        if (processor == mixerStage)
        {
            if (running)
            {
                combineAtMixer (buffer, pathBView);
                running = false;
            }

            continue;
        }

        const auto onBusB = (buses & (1u << (unsigned) index)) != 0;

        processor->processBlock (running && onBusB ? pathBView : buffer);
    }

    // A mixer dragged in front of its split would otherwise leave path B
    // hanging and silently discard everything on it. Fold it back rather than
    // losing half the signal.
    if (running)
        combineAtMixer (buffer, pathBView);

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

    for (const auto& processor : postProcessors)
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

    // A new stage invalidates whatever permutation was in force, so the order
    // returns to the one the chain was built in. Only ever called at build time.
    packedOrder.store (identityOrder(), std::memory_order_release);

    return raw;
}

AudioProcessorBase* DSPChainManager::addPostProcessor (std::unique_ptr<AudioProcessorBase> processor)
{
    if (processor == nullptr)
        return nullptr;

    if (prepared)
        processor->prepareToPlay (currentSampleRate, currentSamplesPerBlock, currentNumChannels);

    auto* raw = processor.get();
    postProcessors.push_back (std::move (processor));
    return raw;
}

void DSPChainManager::clear()
{
    processors.clear();
    postProcessors.clear();
    packedOrder.store (0, std::memory_order_release);
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

juce::uint64 DSPChainManager::identityOrder() const noexcept
{
    juce::uint64 packed = 0;
    const auto count = juce::jmin ((int) processors.size(), kMaxOrderedProcessors);

    for (int i = 0; i < count; ++i)
        packed |= ((juce::uint64) (unsigned) i) << (i * 4);

    return packed;
}

void DSPChainManager::setParallelStages (SplitProcessor* split, MixerProcessor* mixer) noexcept
{
    splitStage = split;
    mixerStage = mixer;
}

bool DSPChainManager::splitIsActive() const noexcept
{
    return splitStage != nullptr && splitStage->isEnabled();
}

void DSPChainManager::divideAtSplit (juce::AudioBuffer<float>& pathA, juce::AudioBuffer<float>& b) noexcept
{
    if (splitStage != nullptr)
        splitStage->divide (pathA, b);
}

void DSPChainManager::combineAtMixer (juce::AudioBuffer<float>& pathA,
                                      const juce::AudioBuffer<float>& b) noexcept
{
    if (mixerStage != nullptr)
        mixerStage->combine (pathA, b);
}

void DSPChainManager::setStageOnBusB (int processorIndex, bool onBusB) noexcept
{
    if (processorIndex < 0 || processorIndex >= kMaxOrderedProcessors)
        return;

    const auto bit = 1u << (unsigned) processorIndex;

    if (onBusB)
        busBMask.fetch_or (bit, std::memory_order_release);
    else
        busBMask.fetch_and (~bit, std::memory_order_release);
}

bool DSPChainManager::isStageOnBusB (int processorIndex) const noexcept
{
    if (processorIndex < 0 || processorIndex >= kMaxOrderedProcessors)
        return false;

    return (busBMask.load (std::memory_order_acquire) & (1u << (unsigned) processorIndex)) != 0;
}

void DSPChainManager::setFixedStages (int leadingFixed, int trailingFixed) noexcept
{
    leadingFixedStages = juce::jmax (0, leadingFixed);
    trailingFixedStages = juce::jmax (0, trailingFixed);
}

bool DSPChainManager::setOrder (const std::vector<int>& order) noexcept
{
    const auto count = (int) processors.size();

    // Beyond sixteen stages a nibble per stage no longer fits in the atomic, so
    // reordering is refused rather than silently corrupting the run order.
    if (count > kMaxOrderedProcessors || (int) order.size() != count)
        return false;

    std::array<bool, kMaxOrderedProcessors> seen {};

    for (int position = 0; position < count; ++position)
    {
        const auto index = order[(size_t) position];

        // A complete permutation, or nothing: a duplicate would run one stage
        // twice and drop another entirely.
        if (index < 0 || index >= count || seen[(size_t) index])
            return false;

        seen[(size_t) index] = true;

        // The pinned head and tail have to stay exactly where they are. The
        // master stage carries the safety limiter, so nothing may follow it.
        const auto isFixedPosition = position < leadingFixedStages
                                     || position >= count - trailingFixedStages;

        if (isFixedPosition && index != position)
            return false;
    }

    juce::uint64 packed = 0;

    for (int position = 0; position < count; ++position)
        packed |= ((juce::uint64) (unsigned) order[(size_t) position]) << (position * 4);

    packedOrder.store (packed, std::memory_order_release);
    return true;
}

std::vector<int> DSPChainManager::getOrder() const
{
    const auto packed = packedOrder.load (std::memory_order_acquire);
    const auto count = juce::jmin ((int) processors.size(), kMaxOrderedProcessors);

    std::vector<int> order;
    order.reserve ((size_t) count);

    for (int position = 0; position < count; ++position)
        order.push_back ((int) ((packed >> (position * 4)) & 0xFULL));

    return order;
}
} // namespace milodikfx::dsp
