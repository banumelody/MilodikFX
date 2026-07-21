#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <limits>

#include "dsp/AudioProcessorBase.h"
#include "dsp/Biquad.h"

namespace milodikfx::dsp
{
/**
 * Three-band tone control: 120 Hz low shelf, 1 kHz peak, 7 kHz high shelf.
 *
 * Coefficients live on the audio thread and are recomputed only when a band's
 * smoothed gain actually moved, so there is no allocation in processBlock and
 * no coefficient sharing across threads.
 */
class EQProcessor final : public AudioProcessorBase
{
public:
    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels) override;
    void processBlock (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setBassDb (float db) noexcept;
    float getBassDb() const noexcept;

    void setMidDb (float db) noexcept;
    float getMidDb() const noexcept;

    void setTrebleDb (float db) noexcept;
    float getTrebleDb() const noexcept;

    void setEnabled (bool shouldEnable) noexcept;
    bool isEnabled() const noexcept;

private:
    // A band is recomputed once its smoothed gain has drifted further than this
    // from the value the current coefficients were built for.
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

    std::atomic<float> bassDb { 0.0f };
    std::atomic<float> midDb { 0.0f };
    std::atomic<float> trebleDb { 0.0f };
    std::atomic<bool> enabled { true };

    double currentSampleRate = 0.0;
    int currentNumChannels = 0;
    bool prepared = false;

    Band bass;
    Band mid;
    Band treble;
};
} // namespace milodikfx::dsp
