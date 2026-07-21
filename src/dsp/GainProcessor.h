#pragma once

#include <JuceHeader.h>

#include "dsp/AudioProcessorBase.h"
#include "dsp/Biquad.h"

namespace milodikfx::dsp
{
/** Clean boost: 0..+24 dB, smoothed so REST-driven changes do not zipper. */
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

    SmoothedParam smoothedGain;
};
} // namespace milodikfx::dsp
