#pragma once

#include "AudioProcessorBase.h"
#include "Biquad.h"

#include <atomic>

namespace milodikfx::dsp {

/**
 * @brief Stereo-linked feed-forward compressor with a dB-domain gain computer.
 *
 * The detector is shared across channels so the stereo image does not shift,
 * and the envelope is clamped//guarded so a bad parameter combination can never
 * latch a non-finite value into the signal path.
 */
class CompressorProcessor final : public AudioProcessorBase
{
public:
    CompressorProcessor() = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels) override;
    void processBlock (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    // Parameter setters
    void setInputGainDb (float db) noexcept;
    void setThresholdDb (float db) noexcept;
    void setRatio (float ratio) noexcept;
    void setAttackMs (float ms) noexcept;
    void setReleaseMs (float ms) noexcept;
    void setAutoMakeupGain (bool enabled) noexcept;
    void setEnabled (bool enabled) noexcept;

    /** Blend of compressed against untouched signal. 100 = fully compressed. */
    void setMixPercent (float percent) noexcept;
    float getMixPercent() const noexcept;

    // Parameter getters
    float getInputGainDb() const noexcept;
    float getThresholdDb() const noexcept;
    float getRatio() const noexcept;
    float getAttackMs() const noexcept;
    float getReleaseMs() const noexcept;
    bool getAutoMakeupGain() const noexcept;
    bool isEnabled() const noexcept;

    /** Most recent gain reduction in dB (<= 0). Safe to read from any thread. */
    float getGainReductionDb() const noexcept;

private:
    // Envelope floor: the detector never reports quieter than this, which keeps
    // log10 away from zero without needing an epsilon fudge in the signal.
    static constexpr float kDetectorFloorDb = -120.0f;

    // Upper bound for the per-sample dry copy held on the stack.
    static constexpr int kMaxChannels = 8;

    double sampleRate = 44100.0;
    std::atomic<float> inputGainDb { 0.0f };
    std::atomic<float> thresholdDb { -24.0f };
    std::atomic<float> ratio { 4.0f };
    std::atomic<float> attackMs { 10.0f };
    std::atomic<float> releaseMs { 100.0f };
    std::atomic<bool> autoMakeupGain { true };
    std::atomic<bool> enabled { true };
    std::atomic<bool> timingDirty { true };
    std::atomic<float> gainReductionDb { 0.0f };
    std::atomic<float> mixPercent { 100.0f };

    SmoothedParam smoothedMix;

    // Audio-thread-owned state.
    float envelopeDb = 0.0f;
    float alphaAttack = 1.0f;
    float alphaRelease = 1.0f;
    float lastAttackMs = -1.0f;
    float lastReleaseMs = -1.0f;

    void updateCoefficientsIfNeeded() noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompressorProcessor)
};

} // namespace milodikfx::dsp
