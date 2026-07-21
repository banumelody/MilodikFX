#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <vector>

#include "dsp/AudioProcessorBase.h"
#include "dsp/Biquad.h"

namespace milodikfx::dsp
{
/**
 * Feedback delay with a fractional read position.
 *
 * The delay time is smoothed and read with linear interpolation, so dragging
 * the time control glides the way a real analogue delay does instead of
 * clicking on every buffer-length change.
 */
class DelayProcessor final : public AudioProcessorBase
{
public:
    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels) override;
    void processBlock (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setTimeMs (float ms) noexcept;
    float getTimeMs() const noexcept;

    void setFeedbackPercent (float percent) noexcept;
    float getFeedbackPercent() const noexcept;

    void setMixPercent (float percent) noexcept;
    float getMixPercent() const noexcept;

    void setEnabled (bool shouldEnable) noexcept;
    bool isEnabled() const noexcept;

private:
    static constexpr float kMaxDelayMs = 1000.0f;

    double sampleRate = 44100.0;
    int currentNumChannels = 0;
    int bufferLength = 0;
    int writeIndex = 0;
    bool prepared = false;

    std::atomic<float> timeMs { 350.0f };
    std::atomic<float> feedbackPercent { 30.0f };
    std::atomic<float> mixPercent { 25.0f };
    std::atomic<bool> enabled { false };

    std::vector<std::vector<float>> lines;

    SmoothedParam smoothedDelaySamples;
    SmoothedParam smoothedFeedback;
    SmoothedParam smoothedMix;
};
} // namespace milodikfx::dsp
