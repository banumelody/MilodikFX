#pragma once

#include "AudioProcessorBase.h"
#include <array>
#include <atomic>
#include <vector>

namespace milodikfx::dsp {

/**
 * @brief Freeverb-style Schroeder reverberator (8 combs + 4 allpasses per side).
 *
 * Comb feedback is derived from the decay time via the RT60 relation, and the
 * right-hand delay lines are offset by the classic stereo spread so the width
 * control has something real to work with. All derived state is recomputed on
 * the audio thread behind a dirty flag, so REST threads only ever touch atomics.
 */
class ReverbProcessor final : public AudioProcessorBase
{
public:
    ReverbProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels) override;
    void processBlock (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    // Parameter setters
    void setRoomSize (float size) noexcept;      // 0.0-1.0
    void setDryWetMix (float mix) noexcept;      // 0.0-1.0 (wet amount)
    void setDecayTime (float seconds) noexcept;  // 0.5-10.0
    void setWidth (float width) noexcept;        // 0.0-1.0
    void setEnabled (bool enabled) noexcept;

    // Parameter getters
    float getRoomSize() const noexcept;
    float getDryWetMix() const noexcept;
    float getDecayTime() const noexcept;
    float getWidth() const noexcept;
    bool isEnabled() const noexcept;

private:
    static constexpr int kNumCombFilters = 8;
    static constexpr int kNumAllpassFilters = 4;
    static constexpr int kCombFilterTuning[] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
    static constexpr int kAllpassFilterTuning[] = { 556, 441, 341, 225 };

    // Freeverb's stereo spread, in samples at 44.1 kHz.
    static constexpr int kStereoSpread = 23;

    // Freeverb's input attenuation. Without it eight parallel combs with high
    // feedback sum to a wet path roughly 20 dB hotter than the dry signal.
    static constexpr float kFixedGain = 0.015f;

    // Freeverb's wet scaling, applied after kFixedGain.
    static constexpr float kWetScale = 3.0f;

    /** Highest rate the delay lines are sized for; see DelayLine::allocate. */
    static constexpr double kMaxSampleRate = 192000.0;

    /**
     * Storage is allocated once for the highest sample rate and never resized;
     * a rate change only moves activeLength. prepareToPlay used to reallocate
     * these, which freed memory the audio thread was still reading.
     */
    struct DelayLine
    {
        std::vector<float> buffer;
        int activeLength = 1;
        int bufferIndex = 0;

        void allocate (int tuningAt44100);
        void setLengthForRate (double sampleRate);
    };

    struct CombFilter : DelayLine
    {
        float filterStore = 0.0f;
        float damp1 = 0.0f;
        float damp2 = 1.0f;
        float feedback = 0.5f;

        void setDamp (float damp) noexcept
        {
            damp1 = damp;
            damp2 = 1.0f - damp;
        }

        void setFeedback (float fb) noexcept { feedback = fb; }
        float process (float input) noexcept;
        void reset() noexcept;
    };

    struct AllpassFilter : DelayLine
    {
        float feedback = 0.5f;

        void setFeedback (float fb) noexcept { feedback = fb; }
        float process (float input) noexcept;
        void reset() noexcept;
    };

    double sampleRate = 44100.0;
    std::atomic<float> roomSize { 0.5f };
    std::atomic<float> dryWetMix { 0.25f };
    std::atomic<float> decayTime { 2.0f };
    std::atomic<float> width { 1.0f };
    std::atomic<bool> enabled { true };
    std::atomic<bool> parametersDirty { true };

    std::array<CombFilter, kNumCombFilters> combL;
    std::array<CombFilter, kNumCombFilters> combR;
    std::array<AllpassFilter, kNumAllpassFilters> allpassL;
    std::array<AllpassFilter, kNumAllpassFilters> allpassR;

    // Audio-thread-owned derived state.
    float dry = 1.0f;
    float wet1 = 0.0f;
    float wet2 = 0.0f;

    void updateParametersIfNeeded() noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbProcessor)
};

} // namespace milodikfx::dsp
