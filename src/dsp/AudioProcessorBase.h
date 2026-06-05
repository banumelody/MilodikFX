#pragma once

#include <JuceHeader.h>

namespace milodikfx::dsp
{
class AudioProcessorBase
{
public:
    virtual ~AudioProcessorBase() = default;

    virtual void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels) = 0;
    virtual void processBlock (juce::AudioBuffer<float>& buffer) = 0;
    virtual void reset() = 0;
};
} // namespace milodikfx::dsp
