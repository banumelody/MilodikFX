#pragma once

#include <JuceHeader.h>

#include <array>
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

    /** Low-pass corner inside the feedback loop; each repeat gets darker. */
    void setDampingHz (float hz) noexcept;
    float getDampingHz() const noexcept;

    /** Crosses the feedback between channels so repeats alternate sides. */
    void setPingPong (bool shouldPingPong) noexcept;
    bool isPingPong() const noexcept;

    /** Note divisions the delay time can be locked to. Index order is the
        order the UI shows, so appending is safe but reordering is not --
        the index is what gets written to presets and settings. */
    enum class SyncDivision
    {
        off = 0,
        quarter,
        eighthDotted,
        eighth,
        eighthTriplet,
        sixteenth
    };

    static constexpr int kNumSyncDivisions = 6;

    void setSyncDivision (int index) noexcept;
    int getSyncDivision() const noexcept;

    void setBpm (float beatsPerMinute) noexcept;
    float getBpm() const noexcept;

    /** The time actually in use: the dialled ms, or the tempo-derived one when
        a division is selected. This is what the UI should display. */
    float getEffectiveTimeMs() const noexcept;

    /**
     * Lets the repeats ring on after the delay is switched off.
     *
     * Switching off stops feeding the line but keeps the tail decaying, so a
     * scene change mid-song does not chop the repeats dead. Once the tail has
     * fallen below -80 dB the block stops processing entirely, so a silent
     * delay costs nothing.
     */
    void setSpillover (bool shouldSpill) noexcept;
    bool isSpillover() const noexcept;

    void setEnabled (bool shouldEnable) noexcept;
    bool isEnabled() const noexcept;

    /** Exposed for testing: false once the tail has decayed and the block idles. */
    bool isTailRinging() const noexcept { return ! tailIdle; }

private:
    static constexpr float kMaxDelayMs = 1000.0f;
    static constexpr double kMaxSampleRate = 192000.0;
    static constexpr int kMaxChannels = 2;

    /** Enough for the longest delay at the highest rate, so it always fits. */
    static constexpr int kLineLength = (int) (kMaxDelayMs * 0.001 * kMaxSampleRate) + 2;

    double sampleRate = 44100.0;
    int writeIndex = 0;
    bool prepared = false;

    /** Above this the damping filter is treated as off, avoiding a needless stage. */
    static constexpr float kDampingOffHz = 19000.0f;

    std::atomic<float> timeMs { 350.0f };
    std::atomic<float> feedbackPercent { 30.0f };
    std::atomic<float> mixPercent { 25.0f };
    std::atomic<float> dampingHz { 20000.0f };
    std::atomic<bool> pingPong { false };
    std::atomic<int> syncDivision { 0 };
    std::atomic<float> bpm { 120.0f };
    std::atomic<bool> spillover { true };
    std::atomic<bool> enabled { false };

    /** How long the input takes to fade out of the line when switching off. */
    static constexpr float kSpilloverFadeSeconds = 0.02f;

    /** -80 dB: below this the tail is inaudible and the block can stop. */
    static constexpr float kTailSilence = 1.0e-4f;

    // Audio-thread owned. 1 = feeding the line, 0 = tail only.
    float activeAmount = 0.0f;
    float activeStep = 1.0f;
    bool tailIdle = true;

    // Fixed size, allocated in the constructor. Never resized.
    std::vector<std::vector<float>> lines;

    // Damping filter state, one per channel, sized once and never reallocated --
    // the same discipline the delay lines follow.
    std::array<BiquadState, kMaxChannels> dampingStates {};
    BiquadCoeffs dampingCoeffs;
    float builtDampingForHz = 0.0f;

    SmoothedParam smoothedDelaySamples;
    SmoothedParam smoothedFeedback;
    SmoothedParam smoothedMix;
};
} // namespace milodikfx::dsp
