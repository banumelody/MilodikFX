#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <limits>
#include <vector>

#include "dsp/AudioProcessorBase.h"
#include "dsp/Biquad.h"

namespace milodikfx::dsp
{
/**
 * Analytic 4x12 guitar cabinet emulation.
 *
 * A DI'd guitar through a clipper without a cabinet is all fizz above 6 kHz and
 * flub below 80 Hz, because a real speaker never reproduces either. This is the
 * cheap analytic stand-in for an impulse response: a steep low cut, the
 * characteristic low thump and lower-mid scoop, a presence peak, and a steep
 * high cut where the speaker stops responding.
 */
class CabinetProcessor final : public AudioProcessorBase
{
public:
    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels) override;
    void processBlock (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setPresenceDb (float db) noexcept;
    float getPresenceDb() const noexcept;

    /** Speaker roll-off corner, 2-8 kHz. Lower is darker/warmer. */
    void setToneHz (float hz) noexcept;
    float getToneHz() const noexcept;

    void setEnabled (bool shouldEnable) noexcept;
    bool isEnabled() const noexcept;

private:
    static constexpr int kNumStages = 6;

    /** Fixed so nothing here is ever reallocated while audio is running. */
    static constexpr int kMaxChannels = 8;

    void rebuildCoefficients (float presenceDb, float toneHz) noexcept;

    double sampleRate = 44100.0;
    int currentNumChannels = 0;
    bool prepared = false;

    std::atomic<float> presenceDb { 0.0f };
    std::atomic<float> toneHz { 5500.0f };
    std::atomic<bool> enabled { true };

    // Audio-thread-owned.
    std::array<BiquadCoeffs, kNumStages> coeffs;
    std::array<std::array<BiquadState, kNumStages>, kMaxChannels> states {};
    float builtForPresenceDb = std::numeric_limits<float>::quiet_NaN();
    float builtForToneHz = std::numeric_limits<float>::quiet_NaN();

    SmoothedParam smoothedPresence;
    SmoothedParam smoothedTone;
};
} // namespace milodikfx::dsp
