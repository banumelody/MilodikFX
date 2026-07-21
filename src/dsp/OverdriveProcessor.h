#pragma once

#include <JuceHeader.h>

#include <memory>

#include "dsp/AudioProcessorBase.h"
#include "dsp/Biquad.h"

namespace milodikfx::dsp
{
/**
 * Cubic soft-clipper with 2x oversampling.
 *
 * The clipper is a hard nonlinearity at high drive, which folds harmonics back
 * below Nyquist at 48 kHz. Running it at 2x with a polyphase IIR half-band
 * filter pushes most of that fizz out of the audible band at a few samples of
 * latency.
 */
class OverdriveProcessor final : public AudioProcessorBase
{
public:
    OverdriveProcessor();
    ~OverdriveProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels) override;
    void processBlock (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setDrivePercent (float percent) noexcept;
    float getDrivePercent() const noexcept;

    void setLevelPercent (float percent) noexcept;
    float getLevelPercent() const noexcept;

    void setEnabled (bool shouldEnable) noexcept;
    bool isEnabled() const noexcept;

    void setOversamplingEnabled (bool shouldEnable) noexcept;
    bool isOversamplingEnabled() const noexcept;

    /** Extra latency introduced by the oversampler, in samples at the device rate. */
    float getLatencySamples() const noexcept;

private:
    static constexpr int kOversampleFactorLog2 = 1; // 2x
    static constexpr int kOversampleRatio = 1 << kOversampleFactorLog2;

    static float softClip (float x) noexcept;

    void applyDrive (float* const* channels, int numChannels, int numSamples, float driveTarget) noexcept;
    void applyLevel (float* const* channels, int numChannels, int numSamples, float levelTarget) noexcept;

    std::atomic<float> drivePercent { 0.0f };   // 0..100
    std::atomic<float> driveAmount { 0.0f };    // 0..1

    std::atomic<float> levelPercent { 100.0f }; // 0..100
    std::atomic<float> levelLinear { 1.0f };    // 0..1

    std::atomic<bool> enabled { true };
    std::atomic<bool> oversamplingEnabled { true };
    bool prepared = false;

    int preparedChannels = 0;
    int preparedBlockSize = 0;

    SmoothedParam smoothedDrive;
    SmoothedParam smoothedLevel;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
};
} // namespace milodikfx::dsp
