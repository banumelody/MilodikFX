#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <memory>

#include "dsp/AudioProcessorBase.h"
#include "dsp/Biquad.h"

namespace milodikfx::dsp
{
/**
 * Cubic soft-clipper with selectable oversampling and adjustable asymmetry.
 *
 * The clipper is a hard nonlinearity at high drive, which folds harmonics back
 * below Nyquist at 48 kHz; oversampling pushes most of that fizz out of the
 * audible band. Asymmetry biases the waveform before clipping so the curve is
 * no longer odd-symmetric, which is what produces the even harmonics a valve
 * stage is known for.
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

    /** 0 = off, 1 = 2x, 2 = 4x, 3 = 8x. Switching never allocates. */
    void setOversamplingIndex (int index) noexcept;
    int getOversamplingIndex() const noexcept;

    /** 0 = symmetric (the classic curve), 1 = maximum valve-like bias. */
    void setAsymmetry (float amount) noexcept;
    float getAsymmetry() const noexcept;

    /** Extra latency introduced by the active oversampler, in samples. */
    float getLatencySamples() const noexcept;

private:
    // One oversampler per factor, all built up front: changing factor mid-stream
    // would otherwise mean allocating on the audio thread.
    static constexpr int kNumOversamplers = 3; // 2x, 4x, 8x
    static constexpr int kMaxBlockChannels = 32;

    /** Largest bias offset applied before clipping at asymmetry = 1. */
    static constexpr float kMaxBias = 0.3f;

    static float softClip (float x) noexcept;

    void applyDrive (float* const* channels,
                     int numChannels,
                     int numSamples,
                     float driveTarget,
                     float asymmetryTarget) noexcept;

    void applyLevel (float* const* channels, int numChannels, int numSamples, float levelTarget) noexcept;

    /** The oversampler for the current index, or nullptr when oversampling is off. */
    juce::dsp::Oversampling<float>* activeOversampler() const noexcept;

    std::atomic<float> drivePercent { 0.0f };   // 0..100
    std::atomic<float> driveAmount { 0.0f };    // 0..1

    std::atomic<float> levelPercent { 100.0f }; // 0..100
    std::atomic<float> levelLinear { 1.0f };    // 0..1

    std::atomic<float> asymmetry { 0.0f };      // 0..1

    std::atomic<bool> enabled { true };
    std::atomic<bool> oversamplingEnabled { true };
    std::atomic<int> oversamplingIndex { 1 };   // default 2x, matching the original behaviour
    bool prepared = false;

    int preparedChannels = 0;
    int preparedBlockSize = 0;

    SmoothedParam smoothedDrive;
    SmoothedParam smoothedLevel;
    SmoothedParam smoothedAsymmetry;

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, kNumOversamplers> oversamplers;
};
} // namespace milodikfx::dsp
