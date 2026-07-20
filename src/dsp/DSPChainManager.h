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
    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels);
    void processBlock (juce::AudioBuffer<float>& buffer);
    void reset();

    AudioProcessorBase* addProcessor (std::unique_ptr<AudioProcessorBase> processor);
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
        return nullptr;
    }

private:
    std::vector<std::unique_ptr<AudioProcessorBase>> processors;
    std::atomic<bool> bypassed { false };
    double currentSampleRate = 0.0;
    int currentSamplesPerBlock = 0;
    int currentNumChannels = 0;
    bool prepared = false;
};
} // namespace milodikfx::dsp
