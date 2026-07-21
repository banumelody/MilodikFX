#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <vector>

namespace milodikfx::dsp
{
/** What the tuner currently thinks it is hearing. */
struct TunerReading
{
    float frequencyHz = 0.0f;
    float cents = 0.0f;      // deviation from the nearest semitone, -50..+50
    float confidence = 0.0f; // 0 = nothing usable, 1 = a clear pitch
    int midiNote = -1;       // -1 when nothing was detected
};

/**
 * Guitar pitch detection using YIN.
 *
 * The audio thread only copies samples into a ring buffer; the analysis itself
 * runs on a worker thread. A YIN pass over 2048 samples is on the order of a
 * million operations, which would blow straight through the 0.67 ms budget of a
 * 32-sample callback if it ran inline.
 *
 * YIN rather than plain autocorrelation because autocorrelation reports the
 * octave below often enough on a wound low E to be a real annoyance.
 */
class TunerAnalyzer final : private juce::Thread
{
public:
    TunerAnalyzer();
    ~TunerAnalyzer() override;

    void prepare (double sampleRate);

    /** Called from the audio thread. Copies only; never blocks or allocates. */
    void pushSamples (const float* samples, int numSamples) noexcept;

    void setEnabled (bool shouldEnable) noexcept;
    bool isEnabled() const noexcept { return enabled.load (std::memory_order_relaxed); }

    TunerReading getReading() const noexcept;

    /** Name of a MIDI note, e.g. "E2". Empty for an invalid note number. */
    static juce::String describeNote (int midiNote);

    /** Exposed for testing: runs YIN over one window and returns the pitch. */
    static float detectPitch (const float* window, int windowSize, double sampleRate);

private:
    // 2048 samples at 48 kHz is ~43 ms: two full periods of a low E (82 Hz),
    // which is the shallowest window that detects it reliably.
    static constexpr int kWindowSize = 2048;
    static constexpr int kRingSize = 8192;

    /** Roughly ten analyses a second; tuning needs no more than that. */
    static constexpr int kAnalysisIntervalMs = 100;

    void run() override;

    std::array<float, kRingSize> ring {};
    std::atomic<int> writeIndex { 0 };
    std::atomic<bool> enabled { false };

    std::atomic<double> sampleRate { 48000.0 };

    // Published as one word each; a reader can only ever see a slightly stale
    // reading, never a torn one.
    std::atomic<float> frequency { 0.0f };
    std::atomic<float> cents { 0.0f };
    std::atomic<float> confidence { 0.0f };
    std::atomic<int> midiNote { -1 };

    std::vector<float> analysisWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerAnalyzer)
};
} // namespace milodikfx::dsp
