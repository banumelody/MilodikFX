#pragma once

#include <JuceHeader.h>

#include "dsp/AudioProcessorBase.h"

namespace milodikfx::dsp
{
class GainProcessor final : public AudioProcessorBase
{
public:
    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels) override;
    void processBlock (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setGainDb (float db) noexcept;
    float getGainDb() const noexcept;

    void setEnabled (bool shouldEnable) noexcept;
    bool isEnabled() const noexcept;

private:
    std::atomic<float> gainDb { 0.0f };
    std::atomic<float> gainLinear { 1.0f };
    std::atomic<bool> enabled { true };
    bool prepared = false;
};
} // namespace milodikfx::dsp
