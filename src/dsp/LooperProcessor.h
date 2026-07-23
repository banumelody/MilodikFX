#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <cmath>

#include "dsp/AudioProcessorBase.h"

namespace milodikfx::dsp
{
/**
 * A single-loop phrase looper, mixed in after the master stage.
 *
 * Like the metronome it is a post-processor, not a stage of the chain: it loops
 * the *processed* output and keeps playing across a global bypass, which is
 * exactly when you want the loop to carry on. It carries its own clamp because it
 * adds level to an already-limited signal.
 *
 * One footswitch drives it -- `record` is context-sensitive: it starts the first
 * pass, closes the loop, then toggles overdub against play. `stop`, `play` and
 * `clear` are explicit. The control thread only ever *requests* an action through
 * an atomic; the audio thread applies it at the top of a block, so nothing is
 * allocated, freed or locked in the callback. The record buffer is allocated once
 * at prepare, large enough for the maximum loop, and `clear` just resets the
 * length -- it never zeroes megabytes on the audio thread.
 */
class LooperProcessor final : public AudioProcessorBase
{
public:
    enum class State
    {
        empty = 0,
        recording = 1,
        playing = 2,
        overdubbing = 3,
        stopped = 4
    };

    /** What the control thread can ask the audio thread to do next block. */
    enum class Action
    {
        none = 0,
        record, // context-sensitive: record -> close -> overdub/play
        stop,
        play,
        clear
    };

    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels) override;
    void processBlock (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    /** Queue an action; the audio thread applies it at the next block. Any thread. */
    void requestAction (Action action) noexcept { pendingAction.store ((int) action, std::memory_order_relaxed); }

    void setLevelPercent (float percent) noexcept
    {
        levelPercent.store (juce::jlimit (0.0f, 100.0f, percent), std::memory_order_relaxed);
    }
    float getLevelPercent() const noexcept { return levelPercent.load (std::memory_order_relaxed); }

    State getState() const noexcept { return (State) publishedState.load (std::memory_order_relaxed); }
    bool hasLoop() const noexcept { return publishedLoopSamples.load (std::memory_order_relaxed) > 0; }

    /** Length of the recorded loop in seconds, 0 when there is none. */
    float getLoopSeconds() const noexcept
    {
        const auto samples = publishedLoopSamples.load (std::memory_order_relaxed);
        return sampleRate > 0.0 ? (float) ((double) samples / sampleRate) : 0.0f;
    }

    /** Playback position through the loop, 0..1. 0 when there is no loop. */
    float getPositionFraction() const noexcept
    {
        const auto len = publishedLoopSamples.load (std::memory_order_relaxed);
        if (len <= 0)
            return 0.0f;

        return juce::jlimit (0.0f, 1.0f, (float) publishedPosition.load (std::memory_order_relaxed) / (float) len);
    }

    /** The longest loop the pre-allocated buffer can hold, so the UI can warn. */
    static constexpr double kMaxSeconds = 60.0;

private:
    void applyAction (Action action) noexcept;
    static float clampSample (float value) noexcept
    {
        if (! std::isfinite (value))
            return 0.0f;

        return juce::jlimit (-1.0f, 1.0f, value);
    }

    double sampleRate = 48000.0;
    bool prepared = false;

    // The record buffer, allocated once at prepare. Never resized on the audio thread.
    juce::AudioBuffer<float> loop;
    int maxSamples = 0;

    std::atomic<int> pendingAction { (int) Action::none };
    std::atomic<float> levelPercent { 100.0f };

    // Published for the UI. Written once per block on the audio thread.
    std::atomic<int> publishedState { (int) State::empty };
    std::atomic<int> publishedLoopSamples { 0 };
    std::atomic<int> publishedPosition { 0 };

    // Audio-thread-owned.
    State state = State::empty;
    int loopLength = 0;
    int recordPos = 0;
    int playPos = 0;
};
} // namespace milodikfx::dsp
