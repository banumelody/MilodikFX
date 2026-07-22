#include "dsp/TunerAnalyzer.h"

#include <cmath>

namespace milodikfx::dsp
{
namespace
{
/** Lowest pitch worth looking for: below a five-string bass low B (B0, 30.9 Hz),
    with margin, so a slightly flat low B still reads rather than dropping out. */
constexpr double kMinFrequencyHz = 27.5;

/** Highest: well above the 24th fret on a guitar's top string. */
constexpr double kMaxFrequencyHz = 1400.0;

/** YIN's absolute threshold. Lower is stricter about what counts as a pitch. */
constexpr float kYinThreshold = 0.15f;

/** Below this RMS the input is treated as silence rather than a bad reading. */
constexpr float kSilenceRms = 0.0015f;

const char* const kNoteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
} // namespace

TunerAnalyzer::TunerAnalyzer()
    : juce::Thread ("MilodikFX Tuner")
{
    analysisWindow.resize (kAnalysisWindowSize, 0.0f);
    hostScratch.resize (kRingSize, 0.0f);

    prepare (48000.0);
    startThread (juce::Thread::Priority::low);
}

TunerAnalyzer::~TunerAnalyzer()
{
    stopThread (2000);
}

void TunerAnalyzer::prepare (double newSampleRate)
{
    const auto host = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    sampleRate.store (host, std::memory_order_relaxed);

    // Decimate by the nearest integer factor to the target rate. The worker
    // reads these plain (non-atomic) fields; prepare() is called from the audio
    // setup on the message thread while analysis is idle, and the worst a race
    // could do is one stale reading, which the next pass corrects.
    decimation = juce::jlimit (1, kMaxDecimation,
                               (int) std::lround (host / kTargetAnalysisRate));
    analysisRate = host / (double) decimation;
    spanHost = juce::jmin (kAnalysisWindowSize * decimation, kRingSize);

    // Anti-alias below the decimated Nyquist before picking every Nth sample,
    // or high-frequency content would fold down into the pitch range and
    // mislead YIN. 0.42 of the analysis rate leaves the whole fundamental range
    // and several harmonics while giving the filter room to roll off.
    antiAliasCoeffs = biquad::makeLowPass (host, analysisRate * 0.42, 0.707);
}

void TunerAnalyzer::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable, std::memory_order_relaxed);

    if (! shouldEnable)
    {
        frequency.store (0.0f, std::memory_order_relaxed);
        confidence.store (0.0f, std::memory_order_relaxed);
        midiNote.store (-1, std::memory_order_relaxed);
        cents.store (0.0f, std::memory_order_relaxed);
    }
}

void TunerAnalyzer::pushSamples (const float* samples, int numSamples) noexcept
{
    if (samples == nullptr || numSamples <= 0 || ! enabled.load (std::memory_order_relaxed))
        return;

    auto index = writeIndex.load (std::memory_order_relaxed);

    for (int i = 0; i < numSamples; ++i)
    {
        ring[(size_t) index] = samples[i];

        if (++index >= kRingSize)
            index = 0;
    }

    writeIndex.store (index, std::memory_order_release);
}

TunerReading TunerAnalyzer::getReading() const noexcept
{
    TunerReading reading;
    reading.frequencyHz = frequency.load (std::memory_order_relaxed);
    reading.cents = cents.load (std::memory_order_relaxed);
    reading.confidence = confidence.load (std::memory_order_relaxed);
    reading.midiNote = midiNote.load (std::memory_order_relaxed);
    return reading;
}

juce::String TunerAnalyzer::describeNote (int note)
{
    if (note < 0 || note > 127)
        return {};

    const auto octave = (note / 12) - 1;
    return juce::String (kNoteNames[note % 12]) + juce::String (octave);
}

float TunerAnalyzer::detectPitch (const float* window, int windowSize, double rate)
{
    if (window == nullptr || windowSize < 128 || rate <= 0.0)
        return 0.0f;

    const auto minLag = juce::jmax (2, (int) (rate / kMaxFrequencyHz));
    const auto maxLag = juce::jmin (windowSize / 2, (int) (rate / kMinFrequencyHz));

    if (maxLag <= minLag)
        return 0.0f;

    // Step 1: the squared difference function.
    std::vector<float> difference ((size_t) maxLag + 1, 0.0f);

    for (int lag = 1; lag <= maxLag; ++lag)
    {
        float sum = 0.0f;

        for (int i = 0; i < windowSize - maxLag; ++i)
        {
            const auto delta = window[i] - window[i + lag];
            sum += delta * delta;
        }

        difference[(size_t) lag] = sum;
    }

    // Step 2: cumulative mean normalisation. This is what stops the function
    // from always preferring lag 0 and is the difference between YIN and plain
    // autocorrelation.
    std::vector<float> normalised ((size_t) maxLag + 1, 1.0f);
    float runningSum = 0.0f;

    for (int lag = 1; lag <= maxLag; ++lag)
    {
        runningSum += difference[(size_t) lag];

        normalised[(size_t) lag] = runningSum > 0.0f
                                       ? difference[(size_t) lag] * (float) lag / runningSum
                                       : 1.0f;
    }

    // Step 3: first dip below the threshold, followed to its local minimum.
    auto chosenLag = -1;

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        if (normalised[(size_t) lag] < kYinThreshold)
        {
            while (lag + 1 <= maxLag && normalised[(size_t) (lag + 1)] < normalised[(size_t) lag])
                ++lag;

            chosenLag = lag;
            break;
        }
    }

    if (chosenLag < 0)
        return 0.0f;

    // Step 4: parabolic interpolation around the minimum, so the estimate is not
    // quantised to whole samples -- without this, resolution at the top of the
    // range is far worse than the few cents a tuner needs.
    auto refinedLag = (float) chosenLag;

    if (chosenLag > 1 && chosenLag < maxLag)
    {
        const auto before = normalised[(size_t) (chosenLag - 1)];
        const auto at = normalised[(size_t) chosenLag];
        const auto after = normalised[(size_t) (chosenLag + 1)];

        const auto denominator = 2.0f * (2.0f * at - before - after);

        if (std::abs (denominator) > 1.0e-9f)
            refinedLag += (after - before) / denominator;
    }

    if (refinedLag <= 0.0f)
        return 0.0f;

    const auto hz = (float) (rate / (double) refinedLag);

    if (! std::isfinite (hz) || hz < (float) kMinFrequencyHz || hz > (float) kMaxFrequencyHz)
        return 0.0f;

    return hz;
}

void TunerAnalyzer::run()
{
    while (! threadShouldExit())
    {
        wait (kAnalysisIntervalMs);

        if (threadShouldExit())
            return;

        if (! enabled.load (std::memory_order_relaxed))
            continue;

        const auto end = writeIndex.load (std::memory_order_acquire);

        const auto span = juce::jmax (1, spanHost);

        // Copy the most recent span of host samples out of the ring. A sample
        // being rewritten underneath us costs at worst one bad reading, which
        // the next pass corrects; the ring's margin over the span makes even
        // that vanishingly unlikely.
        auto readIndex = end - span;

        while (readIndex < 0)
            readIndex += kRingSize;

        double sumSquares = 0.0;

        for (int i = 0; i < span; ++i)
        {
            const auto s = ring[(size_t) readIndex];
            hostScratch[(size_t) i] = s;
            sumSquares += (double) s * (double) s;

            if (++readIndex >= kRingSize)
                readIndex = 0;
        }

        const auto rms = (float) std::sqrt (sumSquares / (double) span);

        if (rms < kSilenceRms)
        {
            confidence.store (0.0f, std::memory_order_relaxed);
            midiNote.store (-1, std::memory_order_relaxed);
            frequency.store (0.0f, std::memory_order_relaxed);
            continue;
        }

        // Anti-alias, then decimate to the analysis rate by taking every Nth
        // filtered sample. A fresh filter each pass has a few-sample warm-up
        // transient, negligible against a window of thousands.
        BiquadState filterState;

        for (int i = 0; i < span; ++i)
            hostScratch[(size_t) i] = filterState.process (antiAliasCoeffs, hostScratch[(size_t) i]);

        for (int k = 0; k < kAnalysisWindowSize; ++k)
        {
            const auto src = k * decimation;
            analysisWindow[(size_t) k] = src < span ? hostScratch[(size_t) src] : 0.0f;
        }

        const auto hz = detectPitch (analysisWindow.data(), kAnalysisWindowSize, analysisRate);

        if (hz <= 0.0f)
        {
            confidence.store (0.0f, std::memory_order_relaxed);
            midiNote.store (-1, std::memory_order_relaxed);
            frequency.store (0.0f, std::memory_order_relaxed);
            continue;
        }

        const auto midi = 69.0 + 12.0 * std::log2 ((double) hz / 440.0);
        const auto nearest = (int) std::lround (midi);
        const auto deviation = (float) ((midi - (double) nearest) * 100.0);

        frequency.store (hz, std::memory_order_relaxed);
        midiNote.store (juce::jlimit (0, 127, nearest), std::memory_order_relaxed);
        cents.store (deviation, std::memory_order_relaxed);
        confidence.store (juce::jlimit (0.0f, 1.0f, rms * 20.0f), std::memory_order_relaxed);
    }
}
} // namespace milodikfx::dsp
