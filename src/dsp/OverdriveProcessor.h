#pragma once

#include <JuceHeader.h>

#include "dsp/AudioProcessorBase.h"

namespace milodikfx::dsp
{
class OverdriveProcessor final : public AudioProcessorBase
{
public:
    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels) override;
    void processBlock (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setDrivePercent (float percent) noexcept;
    float getDrivePercent() const noexcept;

    void setLevelPercent (float percent) noexcept;
    float getLevelPercent() const noexcept;

    void setEnabled (bool shouldEnable) noexcept;
    bool isEnabled() const noexcept;

private:
    static float softClip (float x) noexcept;

    std::atomic<float> drivePercent { 0.0f };   // 0..100
    std::atomic<float> driveAmount { 0.0f };    // 0..1

    std::atomic<float> levelPercent { 100.0f }; // 0..100
    std::atomic<float> levelLinear { 1.0f };    // 0..1

    std::atomic<bool> enabled { true };
    bool prepared = false;
};
} // namespace milodikfx::dsp
