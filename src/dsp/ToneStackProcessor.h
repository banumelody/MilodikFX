#pragma once

#include "AudioProcessorBase.h"
#include "Biquad.h"

#include <array>
#include <atomic>
#include <limits>

namespace milodikfx::dsp {

/**
 * @brief Post-cabinet contour: 50 Hz / 500 Hz / 5 kHz peaking bands.
 *
 * Deliberately centred away from EQProcessor's 120 Hz / 1 kHz / 7 kHz so the
 * two stages shape different parts of the spectrum rather than overlapping.
 * Same coefficient discipline as EQProcessor: audio-thread-owned, recomputed
 * only on change, no allocation in processBlock.
 */
class ToneStackProcessor final : public AudioProcessorBase
{
public:
    ToneStackProcessor() = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels) override;
    void processBlock (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    // Parameter setters
    void setBassDb (float db) noexcept;      // +/-12 dB @ 50 Hz
    void setMidDb (float db) noexcept;       // +/-12 dB @ 500 Hz
    void setTrebleDb (float db) noexcept;    // +/-12 dB @ 5 kHz
    void setEnabled (bool enabled) noexcept;

    // Parameter getters
    float getBassDb() const noexcept;
    float getMidDb() const noexcept;
    float getTrebleDb() const noexcept;
    bool isEnabled() const noexcept;

private:
    static constexpr float kRecomputeThresholdDb = 0.01f;

    /** Fixed so nothing here is ever reallocated while audio is running. */
    static constexpr int kMaxChannels = 8;

    struct Band
    {
        SmoothedParam smoothed;
        BiquadCoeffs coeffs;
        std::array<BiquadState, kMaxChannels> states {};
        float builtForDb = std::numeric_limits<float>::quiet_NaN();
    };

    void snapToTargets();

    double sampleRate = 44100.0;
    int currentNumChannels = 0;
    bool prepared = false;

    std::atomic<float> bassDb { 0.0f };
    std::atomic<float> midDb { 0.0f };
    std::atomic<float> trebleDb { 0.0f };
    std::atomic<bool> enabled { true };

    Band bass;
    Band mid;
    Band treble;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneStackProcessor)
};

} // namespace milodikfx::dsp
