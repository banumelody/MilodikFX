#pragma once

#include "AudioProcessorBase.h"
#include <array>
#include <atomic>

namespace milodikfx::dsp {

/**
 * @brief Reverb processor using Schroeder reverberator (allpass + comb filters).
 */
class ReverbProcessor final : public AudioProcessorBase
{
public:
    ReverbProcessor() = default;
    
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
    static constexpr int kCombFilterTuning[] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
    static constexpr int kAllpassFilterTuning[] = {556, 441, 341, 225};

    struct CombFilter
    {
        std::vector<float> buffer;
        int bufferIndex = 0;
        float filterStore = 0.0f;
        float damp1 = 0.0f;
        float damp2 = 0.0f;
        float feedback = 0.5f;

        void setDamp (float damp);
        void setFeedback (float fb);
        float process (float input);
        void reset();
    };

    struct AllpassFilter
    {
        std::vector<float> buffer;
        int bufferIndex = 0;
        float feedback = 0.5f;

        void setFeedback (float fb) noexcept { feedback = fb; }
        float process (float input);
        void reset();
    };

    double sampleRate = 44100.0;
    std::atomic<float> roomSize { 0.5f };
    std::atomic<float> dryWetMix { 0.5f };
    std::atomic<float> decayTime { 2.0f };
    std::atomic<float> width { 1.0f };
    std::atomic<bool> enabled { true };

    std::array<CombFilter, kNumCombFilters> combL;
    std::array<CombFilter, kNumCombFilters> combR;
    std::array<AllpassFilter, kNumAllpassFilters> allpassL;
    std::array<AllpassFilter, kNumAllpassFilters> allpassR;

    float dry = 0.0f;
    float wet1 = 0.0f;
    float wet2 = 0.0f;

    void updateParameters();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbProcessor)
};

} // namespace milodikfx::dsp
