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
 * The delay lines are allocated once, for the highest sample rate this can ever
 * run at, and never resized again. prepareToPlay used to reallocate them, which
 * freed memory the audio thread was still reading during a device change.
 */
class DelayProcessor final : public AudioProcessorBase
{
public:
    DelayProcessor();

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
    static constexpr double kMaxSampleRate = 192000.0;
    static constexpr int kMaxChannels = 2;

    /** Enough for the longest delay at the highest rate, so it always fits. */
    static constexpr int kLineLength = (int) (kMaxDelayMs * 0.001 * kMaxSampleRate) + 2;

    double sampleRate = 44100.0;
    int writeIndex = 0;
    bool prepared = false;

    std::atomic<float> timeMs { 350.0f };
    std::atomic<float> feedbackPercent { 30.0f };
    std::atomic<float> mixPercent { 25.0f };
    std::atomic<bool> enabled { false };

    // Fixed size, allocated in the constructor. Never resized.
    std::vector<std::vector<float>> lines;

    SmoothedParam smoothedDelaySamples;
    SmoothedParam smoothedFeedback;
    SmoothedParam smoothedMix;
};
} // namespace milodikfx::dsp
