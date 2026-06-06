#pragma once

#include "AudioProcessorBase.h"
#include <atomic>

namespace milodikfx::dsp {

/**
 * @brief Dynamic range compressor with adjustable ratio, threshold, attack/release.
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

    // Parameter getters
    float getInputGainDb() const noexcept;
    float getThresholdDb() const noexcept;
    float getRatio() const noexcept;
    float getAttackMs() const noexcept;
    float getReleaseMs() const noexcept;
    bool getAutoMakeupGain() const noexcept;
    bool isEnabled() const noexcept;

private:
    double sampleRate = 44100.0;
    std::atomic<float> inputGainDb { 0.0f };
    std::atomic<float> thresholdDb { -24.0f };
    std::atomic<float> ratio { 4.0f };
    std::atomic<float> attackMs { 10.0f };
    std::atomic<float> releaseMs { 100.0f };
    std::atomic<bool> autoMakeupGain { true };
    std::atomic<bool> enabled { true };

    float envelopeDb = -100.0f;
    float alphaAttack = 0.0f;
    float alphaRelease = 0.0f;

    void updateCoefficients();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompressorProcessor)
};

} // namespace milodikfx::dsp
