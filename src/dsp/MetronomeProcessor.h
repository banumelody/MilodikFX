#pragma once

#include <JuceHeader.h>

#include <atomic>

#include "dsp/AudioProcessorBase.h"

namespace milodikfx::dsp
{
/**
 * Practice click, mixed in after the master stage.
 *
 * Deliberately last in the chain rather than a stage of it: a click that ran
 * through the amp would be distorted, cabinet-filtered and reverbed, and would
 * go silent the moment the guitar was muted -- which is exactly when you still
 * want to hear the beat. It carries its own hard clamp because it adds level to
 * an already limited signal.
 *
 * This also holds the project tempo. It is the one place a BPM lives; the delay
 * is handed the same value so a synced repeat and the click agree.
 */
class MetronomeProcessor final : public AudioProcessorBase
{
public:
    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels) override;
    void processBlock (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setEnabled (bool shouldEnable) noexcept;
    bool isEnabled() const noexcept;

    void setBpm (float beatsPerMinute) noexcept;
    float getBpm() const noexcept;

    void setVolumePercent (float percent) noexcept;
    float getVolumePercent() const noexcept;

    /** Beats per bar; the first of each gets the accented click. 1 disables the accent. */
    void setBeatsPerBar (int beats) noexcept;
    int getBeatsPerBar() const noexcept;

    /** Beats emitted since the last reset. The UI watches this to flash in time. */
    int getBeatCount() const noexcept;

    /** Which beat of the bar was last played, 0-based. */
    int getBeatInBar() const noexcept;

    static constexpr float kMinBpm = 30.0f;
    static constexpr float kMaxBpm = 300.0f;
    static constexpr int kMaxBeatsPerBar = 12;

private:
    static constexpr float kClickHz = 1000.0f;
    static constexpr float kAccentHz = 1600.0f;
    static constexpr float kClickDecaySeconds = 0.035f;
    static constexpr float kClickPeak = 0.6f;

    double sampleRate = 44100.0;
    bool prepared = false;

    std::atomic<bool> enabled { false };
    std::atomic<float> bpm { 120.0f };
    std::atomic<float> volumePercent { 50.0f };
    std::atomic<int> beatsPerBar { 4 };
    std::atomic<int> beatCount { 0 };
    std::atomic<int> beatInBar { 0 };

    // Audio-thread-owned.
    double samplesUntilBeat = 0.0;
    int nextBeatInBar = 0;
    float clickPhase = 0.0f;
    float clickIncrement = 0.0f;
    float clickEnvelope = 0.0f;
    float clickDecay = 0.0f;
};
} // namespace milodikfx::dsp
