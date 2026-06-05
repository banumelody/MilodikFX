#pragma once

#include <JuceHeader.h>

#include "dsp/DSPChainManager.h"

namespace milodikfx::audio
{
class AudioEngine final
{
public:
    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels);
    void processBlock (juce::AudioBuffer<float>& buffer);
    void reset();

    void setBypassed (bool shouldBypass) noexcept;
    bool isBypassed() const noexcept;

    milodikfx::dsp::DSPChainManager& getChain() noexcept;

private:
    milodikfx::dsp::DSPChainManager chain;
};
} // namespace milodikfx::audio
