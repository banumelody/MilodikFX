#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <memory>
#include <vector>

#include "dsp/AudioProcessorBase.h"

namespace milodikfx::dsp
{
class DSPChainManager final
{
public:
    DSPChainManager();

    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels);
    void processBlock (juce::AudioBuffer<float>& buffer);
    void reset();

    AudioProcessorBase* addProcessor (std::unique_ptr<AudioProcessorBase> processor);

    /**
     * Adds a processor that runs after the chain and after the bypass crossfade.
     *
     * For things that are mixed into the output rather than applied to the
     * guitar: the metronome belongs here because global bypass must not silence
     * the click, and because a click routed through the amp and cabinet would
     * not sound like a click.
     */
    AudioProcessorBase* addPostProcessor (std::unique_ptr<AudioProcessorBase> processor);

    void clear();

    void setBypassed (bool shouldBypass) noexcept;
    bool isBypassed() const noexcept;

    int getNumProcessors() const noexcept;

    // Find the first processor of type T in the chain (uses RTTI/dynamic_cast)
    template<typename T>
    T* findProcessor() noexcept
    {
        for (auto& p : processors)
        {
            if (auto* t = dynamic_cast<T*>(p.get()))
                return t;
        }

        for (auto& p : postProcessors)
        {
            if (auto* t = dynamic_cast<T*>(p.get()))
                return t;
        }

        return nullptr;
    }

private:
    void processChain (juce::AudioBuffer<float>& buffer);

    // The dry copy used to crossfade in and out of bypass. Allocated once, at a
    // size no realistic device block will exceed, so toggling bypass never
    // allocates and prepareToPlay never moves memory the audio thread is reading.
    static constexpr int kMaxChannels = 2;
    static constexpr int kMaxBlockSize = 8192;

    /** Crossfade length, in seconds, when entering or leaving bypass. */
    static constexpr double kBypassFadeSeconds = 0.01;

    std::vector<std::unique_ptr<AudioProcessorBase>> processors;
    std::vector<std::unique_ptr<AudioProcessorBase>> postProcessors;
    std::atomic<bool> bypassed { false };
    double currentSampleRate = 0.0;
    int currentSamplesPerBlock = 0;
    int currentNumChannels = 0;
    bool prepared = false;

    juce::AudioBuffer<float> dryCopy;

    // 1 = fully processed, 0 = fully bypassed. Audio-thread owned.
    float wetGain = 1.0f;
    float fadeStep = 1.0f;
};
} // namespace milodikfx::dsp
