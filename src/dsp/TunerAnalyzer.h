#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <vector>

#include "dsp/Biquad.h"

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
 * Guitar and bass pitch detection using YIN.
 *
 * The audio thread only copies samples into a ring buffer; the analysis runs on
 * a worker thread, because a YIN pass is on the order of a million operations
 * and would blow straight through the 0.67 ms budget of a 32-sample callback if
 * it ran inline.
 *
 * The worker first decimates the host signal down to a ~16 kHz analysis rate.
 * That does two things at once. It reaches a five-string bass low B (B0, 30.9 Hz)
 * and a four-string low E (E1, 41.2 Hz), which are below what the old 55 Hz floor
 * or a 2048-sample host-rate window could ever resolve. And it keeps that reach
 * cheap: finding a 31 Hz period at 96 kHz directly would need a window past 6000
 * samples and a YIN pass about ten times more expensive, where the decimated
 * pass costs roughly what the old guitar-only window did while reaching an
 * octave lower. It also makes the cost independent of the device rate -- the
 * same work whether the interface runs at 44.1 or 192 kHz.
 *
 * YIN rather than plain autocorrelation because autocorrelation reports the
 * octave below often enough on a wound low string to be a real annoyance.
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
    /** Samples the YIN pass runs over, at the decimated analysis rate. 3072 at
        ~16 kHz is ~190 ms: nearly six periods of a low B (31 Hz), comfortably
        more than YIN needs to lock onto it. */
    static constexpr int kAnalysisWindowSize = 3072;

    /** The rate the signal is decimated to before analysis. High enough that a
        1.4 kHz top note still has fine cents resolution, low enough that a low B
        fits the window in a handful of periods. */
    static constexpr double kTargetAnalysisRate = 16000.0;

    /** 192 kHz / 16 kHz. Bounds the ring and the decimation factor. */
    static constexpr int kMaxDecimation = 12;

    /** Host-rate samples the ring holds: the widest span plus margin so the
        audio thread cannot overwrite the oldest sample mid-copy. */
    static constexpr int kRingSize = kAnalysisWindowSize * (kMaxDecimation + 4);

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

    // Worker-thread-owned decimation state, recomputed in prepare().
    int decimation = 3;
    double analysisRate = 16000.0;
    int spanHost = kAnalysisWindowSize * 3;
    BiquadCoeffs antiAliasCoeffs;

    std::vector<float> hostScratch;    // spanHost host-rate samples
    std::vector<float> analysisWindow; // kAnalysisWindowSize decimated samples

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerAnalyzer)
};
} // namespace milodikfx::dsp
