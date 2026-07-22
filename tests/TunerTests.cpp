#include <JuceHeader.h>

#include <cmath>
#include <vector>

#include "dsp/ChainFactory.h"
#include "dsp/DSPChainManager.h"
#include "dsp/DelayProcessor.h"
#include "dsp/MetronomeProcessor.h"
#include "dsp/TunerAnalyzer.h"

namespace
{
constexpr double kRate = 48000.0;

/** Cents between two frequencies; the unit a tuner is actually judged in. */
double centsBetween (double measured, double reference)
{
    return 1200.0 * std::log2 (measured / reference);
}

std::vector<float> makeSine (double freqHz, int numSamples, double sampleRate = kRate, float amplitude = 0.5f)
{
    std::vector<float> window ((size_t) numSamples, 0.0f);

    for (int i = 0; i < numSamples; ++i)
        window[(size_t) i] = amplitude
                             * (float) std::sin (juce::MathConstants<double>::twoPi * freqHz * (double) i / sampleRate);

    return window;
}

/**
 * Streams a continuous sine into the analyzer, block by block, until it locks
 * on or the attempts run out.
 *
 * Phase-continuous on purpose: the analysis window now spans several pushed
 * blocks, so repeating one fixed block would put a discontinuity every block
 * boundary and hand YIN a signal no instrument produces.
 */
milodikfx::dsp::TunerReading feedUntilDetected (milodikfx::dsp::TunerAnalyzer& analyzer,
                                                double freqHz,
                                                double sampleRate,
                                                int wantMidi,
                                                int maxAttempts = 80)
{
    constexpr int kBlock = 1024;
    milodikfx::dsp::TunerReading reading;
    auto startSample = 0;

    for (int attempt = 0; attempt < maxAttempts && reading.midiNote != wantMidi; ++attempt)
    {
        std::vector<float> block ((size_t) kBlock, 0.0f);

        for (int i = 0; i < kBlock; ++i)
            block[(size_t) i] = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi
                                                         * freqHz * (double) (startSample + i) / sampleRate);

        startSample += kBlock;

        analyzer.pushSamples (block.data(), kBlock);
        juce::Thread::sleep (20);
        reading = analyzer.getReading();
    }

    return reading;
}

/** A crude plucked-string spectrum: fundamental plus decaying harmonics. */
std::vector<float> makeHarmonicTone (double freqHz, int numSamples, int numHarmonics = 6)
{
    std::vector<float> window ((size_t) numSamples, 0.0f);

    for (int h = 1; h <= numHarmonics; ++h)
    {
        const auto amplitude = 0.5f / (float) h;

        for (int i = 0; i < numSamples; ++i)
            window[(size_t) i] += amplitude
                                  * (float) std::sin (juce::MathConstants<double>::twoPi * freqHz * (double) h
                                                      * (double) i / kRate);
    }

    return window;
}
} // namespace

//==============================================================================
class TunerAnalyzerTests final : public juce::UnitTest
{
public:
    TunerAnalyzerTests() : juce::UnitTest ("TunerAnalyzer", "dsp") {}

    void runTest() override
    {
        using milodikfx::dsp::TunerAnalyzer;

        // Standard tuning, low to high. These are the only six frequencies the
        // tuner has to get exactly right.
        const struct { const char* name; double hz; } strings[] = {
            { "E2", 82.4069 },
            { "A2", 110.0000 },
            { "D3", 146.8324 },
            { "G3", 195.9977 },
            { "B3", 246.9417 },
            { "E4", 329.6276 },
        };

        beginTest ("detects each open string within 2 cents");
        {
            for (const auto& s : strings)
            {
                const auto window = makeSine (s.hz, 2048);
                const auto detected = TunerAnalyzer::detectPitch (window.data(), (int) window.size(), kRate);

                expect (detected > 0.0f, juce::String ("no pitch detected for ") + s.name);

                const auto error = std::abs (centsBetween ((double) detected, s.hz));

                expect (error < 2.0,
                        juce::String (s.name) + ": detected " + juce::String (detected, 3)
                            + " Hz, off by " + juce::String (error, 2) + " cents");
            }
        }

        beginTest ("does not drop an octave on a harmonically rich note");
        {
            // The failure autocorrelation is prone to: a note with strong
            // harmonics reported an octave low, which on a wound low E means the
            // tuner is confidently wrong.
            for (const auto& s : strings)
            {
                const auto window = makeHarmonicTone (s.hz, 2048);
                const auto detected = TunerAnalyzer::detectPitch (window.data(), (int) window.size(), kRate);

                expect (detected > 0.0f, juce::String ("no pitch for harmonic ") + s.name);

                const auto error = std::abs (centsBetween ((double) detected, s.hz));

                expect (error < 10.0,
                        juce::String (s.name) + " harmonic: detected " + juce::String (detected, 3)
                            + " Hz, off by " + juce::String (error, 2) + " cents");
            }
        }

        beginTest ("resolves a note that is deliberately out of tune");
        {
            // 20 cents sharp of A2 -- the case the whole thing exists for.
            const auto target = 110.0 * std::pow (2.0, 20.0 / 1200.0);
            const auto window = makeSine (target, 2048);
            const auto detected = TunerAnalyzer::detectPitch (window.data(), (int) window.size(), kRate);

            const auto cents = centsBetween ((double) detected, 110.0);
            expect (std::abs (cents - 20.0) < 2.0,
                    "expected about +20 cents, got " + juce::String (cents, 2));
        }

        beginTest ("reports nothing for silence or an unusable window");
        {
            const std::vector<float> silence (2048, 0.0f);
            expectEquals (TunerAnalyzer::detectPitch (silence.data(), 2048, kRate), 0.0f);

            expectEquals (TunerAnalyzer::detectPitch (nullptr, 2048, kRate), 0.0f);

            const auto tiny = makeSine (110.0, 64);
            expectEquals (TunerAnalyzer::detectPitch (tiny.data(), 64, kRate), 0.0f);

            const auto window = makeSine (110.0, 2048);
            expectEquals (TunerAnalyzer::detectPitch (window.data(), 2048, 0.0), 0.0f);
        }

        beginTest ("never reports a pitch outside the range an instrument can produce");
        {
            // Below a five-string bass low B there is nothing musical to find, so
            // it is refused. Measured at the analysis rate the tuner really runs
            // at, where a 20 Hz period is longer than the search reaches.
            const auto tooLow = makeSine (20.0, 3072, 16000.0);
            expectEquals (TunerAnalyzer::detectPitch (tooLow.data(), 3072, 16000.0), 0.0f);

            // Above the last fret, YIN latches onto a subharmonic instead --
            // 3 kHz comes back as 1 kHz. That is not a note any guitar plays and
            // this is not a case the tuner is for; what matters is that whatever
            // comes out is inside the range the display can show. A reading that
            // escaped it would put the needle off the end of its own scale.
            const auto tooHigh = makeSine (3000.0, 2048);
            const auto detected = TunerAnalyzer::detectPitch (tooHigh.data(), 2048, kRate);

            expect (detected == 0.0f || (detected >= 27.5f && detected <= 1400.0f),
                    "reported " + juce::String (detected, 2) + " Hz, outside the instrument range");
        }

        beginTest ("detects every open string of a five-string bass within 2 cents");
        {
            // B-E-A-D-G, low to high: the low B (B0, 30.9 Hz) is the note the old
            // 55 Hz floor could never reach. Measured at the tuner's own analysis
            // rate, where the window holds several periods of even the low B.
            const struct { const char* name; double hz; } bass[] = {
                { "B0", 30.8677 },
                { "E1", 41.2034 },
                { "A1", 55.0000 },
                { "D2", 73.4162 },
                { "G2", 97.9989 },
            };

            for (const auto& s : bass)
            {
                const auto window = makeSine (s.hz, 3072, 16000.0);
                const auto detected = TunerAnalyzer::detectPitch (window.data(), 3072, 16000.0);

                expect (detected > 0.0f, juce::String ("no pitch detected for ") + s.name);

                const auto error = std::abs (centsBetween ((double) detected, s.hz));
                expect (error < 2.0,
                        juce::String (s.name) + ": detected " + juce::String (detected, 3)
                            + " Hz, off by " + juce::String (error, 2) + " cents");
            }
        }

        beginTest ("does not drop a bass low B an octave on a harmonically rich note");
        {
            // The octave error is worst on the lowest, most harmonic-rich notes.
            // Built at the analysis rate: fundamental plus eight decaying harmonics.
            std::vector<float> lowB (3072, 0.0f);
            for (int h = 1; h <= 8; ++h)
                for (int i = 0; i < 3072; ++i)
                    lowB[(size_t) i] += (0.5f / (float) h)
                                        * (float) std::sin (juce::MathConstants<double>::twoPi
                                                            * 30.8677 * (double) h * (double) i / 16000.0);

            const auto detected = TunerAnalyzer::detectPitch (lowB.data(), 3072, 16000.0);

            expect (detected > 24.0f,
                    "detected " + juce::String (detected, 2) + " Hz -- an octave below B0");
            expect (std::abs (centsBetween ((double) detected, 30.8677)) < 12.0);
        }

        beginTest ("takes the shortest period, so a low note is never an octave out");
        {
            // The octave-down error is the one that matters: on a wound low E it
            // makes the tuner confidently wrong. YIN takes the first dip below
            // its threshold, which is the true period rather than a multiple.
            const auto window = makeHarmonicTone (82.4069, 2048, 8);
            const auto detected = TunerAnalyzer::detectPitch (window.data(), 2048, kRate);

            expect (detected > 60.0f,
                    "detected " + juce::String (detected, 2) + " Hz -- an octave below E2");
            expect (std::abs (centsBetween ((double) detected, 82.4069)) < 10.0);
        }

        beginTest ("names notes the way a player would, guitar and bass");
        {
            expectEquals (TunerAnalyzer::describeNote (40), juce::String ("E2"));
            expectEquals (TunerAnalyzer::describeNote (45), juce::String ("A2"));
            expectEquals (TunerAnalyzer::describeNote (60), juce::String ("C4"));
            expectEquals (TunerAnalyzer::describeNote (69), juce::String ("A4"));
            expectEquals (TunerAnalyzer::describeNote (64), juce::String ("E4"));
            // The bass range, down to a five-string low B.
            expectEquals (TunerAnalyzer::describeNote (23), juce::String ("B0"));
            expectEquals (TunerAnalyzer::describeNote (28), juce::String ("E1"));
            expectEquals (TunerAnalyzer::describeNote (43), juce::String ("G2"));
            expect (TunerAnalyzer::describeNote (-1).isEmpty());
            expect (TunerAnalyzer::describeNote (500).isEmpty());
        }

        beginTest ("stays quiet until it is switched on");
        {
            TunerAnalyzer analyzer;
            analyzer.prepare (kRate);

            expect (! analyzer.isEnabled());

            const auto window = makeSine (110.0, 2048);

            // Pushed while disabled, so nothing should ever be reported.
            for (int i = 0; i < 8; ++i)
                analyzer.pushSamples (window.data(), (int) window.size());

            juce::Thread::sleep (350);

            expectEquals (analyzer.getReading().midiNote, -1);
        }

        beginTest ("reports a note pushed from the audio thread once enabled");
        {
            TunerAnalyzer analyzer;
            analyzer.prepare (kRate);
            analyzer.setEnabled (true);

            // A continuous 110 Hz (A2) fed block by block.
            const auto reading = feedUntilDetected (analyzer, 110.0, kRate, 45);

            expectEquals (reading.midiNote, 45); // A2
            // Through the whole decimation + anti-alias path, a few cents of bias
            // is fine -- the display calls anything within 5 cents in tune, and
            // the pure algorithm is held to 2 cents separately above.
            expect (std::abs (reading.cents) < 5.0f,
                    "expected in tune, got " + juce::String (reading.cents, 2) + " cents");
            expect (reading.confidence > 0.0f);

            // Switching off has to clear the reading, or a stale note stays on
            // screen looking live.
            analyzer.setEnabled (false);
            expectEquals (analyzer.getReading().midiNote, -1);
        }

        beginTest ("detects a bass low B through the whole decimation path at 96 kHz");
        {
            // End to end at the rig's actual rate: a B0 (30.9 Hz) pushed at
            // 96 kHz has to survive the ring, the anti-alias filter and the
            // decimation to come back as B0 (MIDI 23). This is the note the old
            // tuner could not see at all.
            TunerAnalyzer analyzer;
            analyzer.prepare (96000.0);
            analyzer.setEnabled (true);

            const auto reading = feedUntilDetected (analyzer, 30.8677, 96000.0, 23);

            expectEquals (reading.midiNote, 23); // B0
            expect (std::abs (reading.cents) < 5.0f,
                    "low B off by " + juce::String (reading.cents, 2) + " cents");
        }

        beginTest ("treats a silent input as no note rather than a bad reading");
        {
            TunerAnalyzer analyzer;
            analyzer.prepare (kRate);
            analyzer.setEnabled (true);

            const std::vector<float> silence (2048, 0.0f);

            for (int i = 0; i < 8; ++i)
            {
                analyzer.pushSamples (silence.data(), (int) silence.size());
                juce::Thread::sleep (25);
            }

            const auto reading = analyzer.getReading();
            expectEquals (reading.midiNote, -1);
            expectEquals (reading.confidence, 0.0f);
        }
    }
};

static TunerAnalyzerTests tunerAnalyzerTests;

//==============================================================================
class MetronomeProcessorTests final : public juce::UnitTest
{
public:
    MetronomeProcessorTests() : juce::UnitTest ("MetronomeProcessor", "dsp") {}

    void runTest() override
    {
        using milodikfx::dsp::MetronomeProcessor;

        beginTest ("adds nothing while switched off");
        {
            MetronomeProcessor metronome;
            metronome.prepareToPlay (kRate, 512, 2);

            juce::AudioBuffer<float> buffer (2, 4800);
            buffer.clear();

            metronome.processBlock (buffer);

            expectEquals (buffer.getMagnitude (0, buffer.getNumSamples()), 0.0f);
            expectEquals (metronome.getBeatCount(), 0);
        }

        beginTest ("clicks once per beat at the tempo it was given");
        {
            MetronomeProcessor metronome;
            metronome.prepareToPlay (kRate, 512, 2);
            metronome.setBpm (120.0f);
            metronome.setEnabled (true);

            // Two seconds at 120 BPM: beats at 0.0, 0.5, 1.0 and 1.5 s.
            juce::AudioBuffer<float> buffer (2, 512);

            for (int block = 0; block < (int) (kRate * 2.0) / 512; ++block)
            {
                buffer.clear();
                metronome.processBlock (buffer);
            }

            expectEquals (metronome.getBeatCount(), 4);
        }

        beginTest ("halves the interval when the tempo doubles");
        {
            MetronomeProcessor metronome;
            metronome.prepareToPlay (kRate, 512, 2);
            metronome.setBpm (240.0f);
            metronome.setEnabled (true);

            juce::AudioBuffer<float> buffer (2, 512);

            for (int block = 0; block < (int) (kRate * 2.0) / 512; ++block)
            {
                buffer.clear();
                metronome.processBlock (buffer);
            }

            expectEquals (metronome.getBeatCount(), 8);
        }

        beginTest ("counts round the bar and back to one");
        {
            MetronomeProcessor metronome;
            metronome.prepareToPlay (kRate, 512, 2);
            metronome.setBpm (120.0f);
            metronome.setBeatsPerBar (3);
            metronome.setEnabled (true);

            juce::AudioBuffer<float> buffer (2, 512);
            std::vector<int> seen;
            auto lastCount = 0;

            for (int block = 0; block < (int) (kRate * 2.0) / 512; ++block)
            {
                buffer.clear();
                metronome.processBlock (buffer);

                if (metronome.getBeatCount() != lastCount)
                {
                    lastCount = metronome.getBeatCount();
                    seen.push_back (metronome.getBeatInBar());
                }
            }

            expectEquals ((int) seen.size(), 4);
            expectEquals (seen[0], 0);
            expectEquals (seen[1], 1);
            expectEquals (seen[2], 2);
            expectEquals (seen[3], 0);
        }

        beginTest ("accents the downbeat louder than the rest of the bar");
        {
            MetronomeProcessor metronome;
            metronome.prepareToPlay (kRate, 512, 2);
            metronome.setBpm (120.0f);
            metronome.setBeatsPerBar (4);
            metronome.setEnabled (true);

            // One beat is 24000 samples; capture the first two separately.
            juce::AudioBuffer<float> first (2, 24000);
            first.clear();
            metronome.processBlock (first);

            juce::AudioBuffer<float> second (2, 24000);
            second.clear();
            metronome.processBlock (second);

            expect (first.getMagnitude (0, first.getNumSamples()) > 0.0f, "downbeat is silent");
            expect (second.getMagnitude (0, second.getNumSamples()) > 0.0f, "second beat is silent");

            // The accent is a pitch difference rather than a level one, so count
            // zero crossings instead of comparing peaks.
            expect (countZeroCrossings (first) > countZeroCrossings (second),
                    "the downbeat should be the higher-pitched click");
        }

        beginTest ("never pushes the output past full scale");
        {
            MetronomeProcessor metronome;
            metronome.prepareToPlay (kRate, 512, 2);
            metronome.setBpm (200.0f);
            metronome.setVolumePercent (100.0f);
            metronome.setEnabled (true);

            juce::AudioBuffer<float> buffer (2, 4800);

            for (int block = 0; block < 20; ++block)
            {
                // Already at the ceiling the limiter would leave it at, which is
                // exactly the case that clips if the click is just added on top.
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    for (int i = 0; i < buffer.getNumSamples(); ++i)
                        buffer.setSample (ch, i, 0.99f);

                metronome.processBlock (buffer);

                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    for (int i = 0; i < buffer.getNumSamples(); ++i)
                    {
                        const auto s = buffer.getSample (ch, i);
                        expect (std::isfinite (s) && s >= -1.0f && s <= 1.0f,
                                "sample out of range: " + juce::String (s));
                    }
            }
        }

        beginTest ("silences the click at zero volume");
        {
            MetronomeProcessor metronome;
            metronome.prepareToPlay (kRate, 512, 2);
            metronome.setBpm (120.0f);
            metronome.setVolumePercent (0.0f);
            metronome.setEnabled (true);

            juce::AudioBuffer<float> buffer (2, 48000);
            buffer.clear();
            metronome.processBlock (buffer);

            expectEquals (buffer.getMagnitude (0, buffer.getNumSamples()), 0.0f);

            // Still counting, so the beat indicator keeps flashing.
            expect (metronome.getBeatCount() > 0);
        }

        beginTest ("clamps the tempo to something playable");
        {
            MetronomeProcessor metronome;

            metronome.setBpm (5.0f);
            expectEquals (metronome.getBpm(), MetronomeProcessor::kMinBpm);

            metronome.setBpm (10000.0f);
            expectEquals (metronome.getBpm(), MetronomeProcessor::kMaxBpm);

            metronome.setBeatsPerBar (0);
            expectEquals (metronome.getBeatsPerBar(), 1);

            metronome.setBeatsPerBar (99);
            expectEquals (metronome.getBeatsPerBar(), MetronomeProcessor::kMaxBeatsPerBar);
        }

        beginTest ("starts on a beat every time it is switched on");
        {
            MetronomeProcessor metronome;
            metronome.prepareToPlay (kRate, 512, 2);
            metronome.setBpm (120.0f);
            metronome.setEnabled (true);

            juce::AudioBuffer<float> buffer (2, 6000);
            buffer.clear();
            metronome.processBlock (buffer);

            metronome.setEnabled (false);
            buffer.clear();
            metronome.processBlock (buffer);

            metronome.setEnabled (true);

            // Switching back on must click immediately rather than finishing a
            // bar that stopped being audible some time ago.
            juce::AudioBuffer<float> restart (2, 256);
            restart.clear();
            metronome.processBlock (restart);

            expect (restart.getMagnitude (0, restart.getNumSamples()) > 0.0f,
                    "no click in the first block after switching back on");
        }
    }

private:
    static int countZeroCrossings (const juce::AudioBuffer<float>& buffer)
    {
        auto crossings = 0;
        const auto* data = buffer.getReadPointer (0);

        for (int i = 1; i < buffer.getNumSamples(); ++i)
            if ((data[i - 1] < 0.0f) != (data[i] < 0.0f))
                ++crossings;

        return crossings;
    }
};

static MetronomeProcessorTests metronomeProcessorTests;

//==============================================================================
class DelaySyncTests final : public juce::UnitTest
{
public:
    DelaySyncTests() : juce::UnitTest ("DelayProcessor tempo sync", "dsp") {}

    void runTest() override
    {
        using milodikfx::dsp::DelayProcessor;

        beginTest ("uses the dialled time when sync is off");
        {
            DelayProcessor delay;
            delay.setTimeMs (375.0f);
            delay.setBpm (120.0f);
            delay.setSyncDivision ((int) DelayProcessor::SyncDivision::off);

            expectWithinAbsoluteError (delay.getEffectiveTimeMs(), 375.0f, 0.01f);
        }

        beginTest ("derives each division from the tempo");
        {
            DelayProcessor delay;
            delay.setTimeMs (375.0f); // deliberately different, and must be ignored
            delay.setBpm (120.0f);    // a quarter note is 500 ms

            const struct { DelayProcessor::SyncDivision division; float expected; } cases[] = {
                { DelayProcessor::SyncDivision::quarter, 500.0f },
                { DelayProcessor::SyncDivision::eighthDotted, 375.0f },
                { DelayProcessor::SyncDivision::eighth, 250.0f },
                { DelayProcessor::SyncDivision::eighthTriplet, 500.0f / 3.0f },
                { DelayProcessor::SyncDivision::sixteenth, 125.0f },
            };

            for (const auto& c : cases)
            {
                delay.setSyncDivision ((int) c.division);
                expectWithinAbsoluteError (delay.getEffectiveTimeMs(), c.expected, 0.01f);
            }
        }

        beginTest ("tracks a tempo change without touching the Time knob");
        {
            DelayProcessor delay;
            delay.setSyncDivision ((int) DelayProcessor::SyncDivision::quarter);

            delay.setBpm (120.0f);
            expectWithinAbsoluteError (delay.getEffectiveTimeMs(), 500.0f, 0.01f);

            delay.setBpm (60.0f);
            expectWithinAbsoluteError (delay.getEffectiveTimeMs(), 1000.0f, 0.01f);

            delay.setBpm (150.0f);
            expectWithinAbsoluteError (delay.getEffectiveTimeMs(), 400.0f, 0.01f);
        }

        beginTest ("keeps a synced time inside what the delay line holds");
        {
            // A quarter note at 30 BPM is 2 s, past the 1 s line. It has to be
            // clamped rather than read off the end.
            DelayProcessor delay;
            delay.setBpm (30.0f);
            delay.setSyncDivision ((int) DelayProcessor::SyncDivision::quarter);

            expectWithinAbsoluteError (delay.getEffectiveTimeMs(), 1000.0f, 0.01f);

            delay.setBpm (300.0f);
            delay.setSyncDivision ((int) DelayProcessor::SyncDivision::sixteenth);

            expect (delay.getEffectiveTimeMs() >= 10.0f);
        }

        beginTest ("refuses a division index that does not exist");
        {
            DelayProcessor delay;

            delay.setSyncDivision (-5);
            expectEquals (delay.getSyncDivision(), 0);

            delay.setSyncDivision (999);
            expectEquals (delay.getSyncDivision(), DelayProcessor::kNumSyncDivisions - 1);
        }

        beginTest ("a synced delay actually repeats at the tempo");
        {
            DelayProcessor delay;
            delay.prepareToPlay (kRate, 512, 2);
            delay.setEnabled (true);
            delay.setBpm (120.0f);
            delay.setSyncDivision ((int) DelayProcessor::SyncDivision::eighth); // 250 ms
            delay.setFeedbackPercent (0.0f);
            delay.setMixPercent (100.0f);

            // An impulse, then silence long enough for the repeat to arrive.
            juce::AudioBuffer<float> buffer (2, (int) (kRate * 0.5));
            buffer.clear();
            buffer.setSample (0, 0, 1.0f);
            buffer.setSample (1, 0, 1.0f);

            delay.processBlock (buffer);

            // The time glides rather than jumping, so the repeat lands near the
            // target rather than exactly on it -- look for the loudest sample in
            // a window around 250 ms.
            auto loudestIndex = 0;
            auto loudest = 0.0f;

            for (int i = 100; i < buffer.getNumSamples(); ++i)
            {
                const auto magnitude = std::abs (buffer.getSample (0, i));

                if (magnitude > loudest)
                {
                    loudest = magnitude;
                    loudestIndex = i;
                }
            }

            const auto seconds = (double) loudestIndex / kRate;

            expect (loudest > 0.1f, "no repeat found at all");
            expect (std::abs (seconds - 0.25) < 0.03,
                    "repeat landed at " + juce::String (seconds, 4) + " s, expected ~0.25");
        }
    }
};

static DelaySyncTests delaySyncTests;

//==============================================================================
class PostChainTests final : public juce::UnitTest
{
public:
    PostChainTests() : juce::UnitTest ("Post-chain processors", "dsp") {}

    void runTest() override
    {
        using milodikfx::dsp::DSPChainManager;
        using milodikfx::dsp::MetronomeProcessor;

        beginTest ("the click survives a global bypass");
        {
            // Bypass exists to A/B the effects. Losing the beat you are playing
            // to every time you compare tones would make it useless in practice.
            DSPChainManager manager;
            auto chain = milodikfx::dsp::buildGuitarChain (manager);

            expect (chain.metronome != nullptr);

            manager.prepareToPlay (kRate, 512, 2);
            manager.setBypassed (true);

            chain.metronome->setBpm (120.0f);
            chain.metronome->setEnabled (true);

            juce::AudioBuffer<float> buffer (2, 512);

            // Let the bypass crossfade settle into its early-out path first.
            for (int block = 0; block < 4; ++block)
            {
                buffer.clear();
                manager.processBlock (buffer);
            }

            expect (manager.isBypassed());

            auto heard = false;

            for (int block = 0; block < 100 && ! heard; ++block)
            {
                buffer.clear();
                manager.processBlock (buffer);
                heard = buffer.getMagnitude (0, buffer.getNumSamples()) > 0.0f;
            }

            expect (heard, "the metronome went silent under global bypass");
        }

        beginTest ("a bypassed chain still passes the guitar through untouched");
        {
            DSPChainManager manager;
            auto chain = milodikfx::dsp::buildGuitarChain (manager);

            manager.prepareToPlay (kRate, 512, 2);
            manager.setBypassed (true);

            chain.metronome->setEnabled (false);

            juce::AudioBuffer<float> buffer (2, 512);

            for (int block = 0; block < 10; ++block)
            {
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < buffer.getNumSamples(); ++i)
                        buffer.setSample (ch, i, 0.25f);

                manager.processBlock (buffer);
            }

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                expectWithinAbsoluteError (buffer.getSample (0, i), 0.25f, 1.0e-6f);
        }

        beginTest ("finds a post-chain processor by type like any other");
        {
            DSPChainManager manager;
            milodikfx::dsp::buildGuitarChain (manager);

            expect (manager.findProcessor<MetronomeProcessor>() != nullptr);
        }
    }
};

static PostChainTests postChainTests;
