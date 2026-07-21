#pragma once

#include <JuceHeader.h>

#include <atomic>

#include "dsp/AudioProcessorBase.h"
#include "dsp/Biquad.h"

namespace milodikfx::dsp
{
/**
 * Final stage: master volume, mute, and a safety limiter.
 *
 * This is the only place in the chain that can turn the signal *down*, and the
 * limiter plus the hard clamp behind it are what stand between a bad knob
 * combination and the interface. Nothing after this point may add gain.
 */
class MasterOutProcessor final : public AudioProcessorBase
{
public:
    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels) override;
    void processBlock (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setVolumeDb (float db) noexcept;
    float getVolumeDb() const noexcept;

    void setMuted (bool shouldMute) noexcept;
    bool isMuted() const noexcept;

    void setLimiterEnabled (bool shouldEnable) noexcept;
    bool isLimiterEnabled() const noexcept;

    void setCeilingDb (float db) noexcept;
    float getCeilingDb() const noexcept;

    /** Limiter gain reduction in dB (<= 0) for the last block. */
    float getLimiterReductionDb() const noexcept;

    static constexpr float kMinVolumeDb = -60.0f;
    static constexpr float kMaxVolumeDb = 12.0f;

private:
    static constexpr float kDetectorFloorDb = -120.0f;
    static constexpr float kAttackMs = 0.5f;
    static constexpr float kReleaseMs = 100.0f;

    void updateCoefficients() noexcept;

    double sampleRate = 44100.0;
    bool prepared = false;

    std::atomic<float> volumeDb { 0.0f };
    std::atomic<float> volumeLinear { 1.0f };
    std::atomic<bool> muted { false };
    std::atomic<bool> limiterEnabled { true };
    std::atomic<float> ceilingDb { -0.3f };
    std::atomic<float> limiterReductionDb { 0.0f };

    // Audio-thread-owned.
    float envelopeDb = 0.0f;
    float alphaAttack = 1.0f;
    float alphaRelease = 1.0f;

    SmoothedParam smoothedVolume;
};
} // namespace milodikfx::dsp
