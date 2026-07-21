#pragma once

#include <JuceHeader.h>

#include <atomic>

#include "dsp/AudioProcessorBase.h"

namespace milodikfx::dsp
{
/**
 * Downward gate placed at the head of the chain, where it sees the raw pickup
 * noise before any boost multiplies it. Detection is stereo-linked, and the
 * hold time stops the gate chattering on a decaying note.
 */
class NoiseGateProcessor final : public AudioProcessorBase
{
public:
    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels) override;
    void processBlock (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setThresholdDb (float db) noexcept;
    float getThresholdDb() const noexcept;

    void setAttackMs (float ms) noexcept;
    float getAttackMs() const noexcept;

    void setHoldMs (float ms) noexcept;
    float getHoldMs() const noexcept;

    void setReleaseMs (float ms) noexcept;
    float getReleaseMs() const noexcept;

    void setEnabled (bool shouldEnable) noexcept;
    bool isEnabled() const noexcept;

    /** Current gate gain, 0 (closed) to 1 (open). Safe from any thread. */
    float getCurrentGain() const noexcept;

private:
    void updateCoefficientsIfNeeded() noexcept;

    double sampleRate = 44100.0;
    bool prepared = false;

    std::atomic<float> thresholdDb { -55.0f };
    std::atomic<float> attackMs { 2.0f };
    std::atomic<float> holdMs { 60.0f };
    std::atomic<float> releaseMs { 150.0f };
    std::atomic<bool> enabled { false };
    std::atomic<bool> timingDirty { true };
    std::atomic<float> currentGain { 1.0f };

    // Audio-thread-owned state.
    float gain = 1.0f;
    int holdCounter = 0;
    int holdSamples = 0;
    float alphaAttack = 1.0f;
    float alphaRelease = 1.0f;
    float lastAttackMs = -1.0f;
    float lastReleaseMs = -1.0f;
    float lastHoldMs = -1.0f;
};
} // namespace milodikfx::dsp
