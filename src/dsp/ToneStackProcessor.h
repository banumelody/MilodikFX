#pragma once

#include "AudioProcessorBase.h"
#include <atomic>

namespace milodikfx::dsp {

/**
 * @brief 3-band presence EQ (Bass/Mid/Treble) - alternative to standard EQ.
 * Emphasizes presence at key frequencies rather than traditional shelving.
 */
class ToneStackProcessor final : public AudioProcessorBase
{
public:
    ToneStackProcessor() = default;
    
    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels) override;
    void processBlock (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    // Parameter setters
    void setBassDb (float db) noexcept;      // ±12 dB @ 50 Hz (low presence)
    void setMidDb (float db) noexcept;       // ±12 dB @ 500 Hz (mid presence)
    void setTrebleDb (float db) noexcept;    // ±12 dB @ 5 kHz (high presence)
    void setEnabled (bool enabled) noexcept;

    // Parameter getters
    float getBassDb() const noexcept;
    float getMidDb() const noexcept;
    float getTrebleDb() const noexcept;
    bool isEnabled() const noexcept;

private:
    double sampleRate = 44100.0;
    std::atomic<float> bassDb { 0.0f };
    std::atomic<float> midDb { 0.0f };
    std::atomic<float> trebleDb { 0.0f };
    std::atomic<bool> enabled { true };

    // Biquad filter states (per channel, per band)
    struct BiquadState
    {
        float x1 = 0.0f, x2 = 0.0f;
        float y1 = 0.0f, y2 = 0.0f;
    };

    // Bass peaking @ 50 Hz
    struct BassFilter
    {
        float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        std::vector<BiquadState> states;
    } bassFilter;

    // Mid peaking @ 500 Hz
    struct MidFilter
    {
        float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        std::vector<BiquadState> states;
    } midFilter;

    // Treble peaking @ 5 kHz
    struct TrebleFilter
    {
        float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        std::vector<BiquadState> states;
    } trebleFilter;

    float processBiquad (float input, float& state_x1, float& state_x2, float& state_y1, float& state_y2,
                         float b0, float b1, float b2, float a1, float a2);
    void updateBassCoefficients();
    void updateMidCoefficients();
    void updateTrebleCoefficients();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneStackProcessor)
};

} // namespace milodikfx::dsp
