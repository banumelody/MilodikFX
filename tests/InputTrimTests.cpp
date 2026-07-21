#include <JuceHeader.h>

#include <cmath>

#include "dsp/ChainFactory.h"
#include "dsp/DSPChainManager.h"
#include "dsp/DelayProcessor.h"
#include "dsp/InputTrimProcessor.h"
#include "dsp/NoiseGateProcessor.h"
#include "dsp/ReverbProcessor.h"

namespace
{
constexpr double kRate = 48000.0;

void fillSine (juce::AudioBuffer<float>& buffer, double freqHz, float amplitude, int startSample = 0)
{
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto phase = juce::MathConstants<double>::twoPi * freqHz
                           * (double) (i + startSample) / kRate;
        const auto s = (float) (amplitude * std::sin (phase));

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample (ch, i, s);
    }
}

float peakOf (const juce::AudioBuffer<float>& buffer)
{
    return buffer.getMagnitude (0, buffer.getNumSamples());
}

/** Runs a processor long enough for any smoothing to settle, then measures. */
float settledPeak (milodikfx::dsp::InputTrimProcessor& trim, float amplitude)
{
    juce::AudioBuffer<float> buffer (2, 4096);

    for (int block = 0; block < 4; ++block)
    {
        fillSine (buffer, 220.0, amplitude);
        trim.processBlock (buffer);
    }

    return buffer.getMagnitude (2048, 2048);
}
} // namespace

//==============================================================================
class InputTrimProcessorTests final : public juce::UnitTest
{
public:
    InputTrimProcessorTests() : juce::UnitTest ("InputTrimProcessor", "dsp") {}

    void runTest() override
    {
        using milodikfx::dsp::InputTrimProcessor;

        beginTest ("At the default it is a bit-identical passthrough");
        {
            // The default must not colour anything at all. A multiply by 1.0f
            // would be almost right; this checks it is actually untouched.
            InputTrimProcessor trim;
            trim.prepareToPlay (kRate, 512, 2);

            juce::AudioBuffer<float> buffer (2, 512);
            fillSine (buffer, 440.0, 0.37f);

            juce::AudioBuffer<float> original (2, 512);
            original.makeCopyOf (buffer);

            trim.processBlock (buffer);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    expect (buffer.getSample (ch, i) == original.getSample (ch, i),
                            "sample " + juce::String (i) + " changed at 0 dB");
        }

        beginTest ("Cuts and boosts by the dB it was given");
        {
            const struct { float db; float factor; } cases[] = {
                { -12.0f, 0.25118864f },
                { -6.0f, 0.50118723f },
                { 0.0f, 1.0f },
                { 6.0f, 1.9952623f },
                { 12.0f, 3.9810717f },
            };

            for (const auto& c : cases)
            {
                InputTrimProcessor trim;
                trim.setGainDb (c.db);
                trim.prepareToPlay (kRate, 4096, 2);

                const auto measured = settledPeak (trim, 0.2f);

                expectWithinAbsoluteError (measured, 0.2f * c.factor, 0.002f,
                                           juce::String (c.db) + " dB was not applied");
            }
        }

        beginTest ("Refuses a range no guitar needs, and rubbish");
        {
            InputTrimProcessor trim;

            trim.setGainDb (999.0f);
            expectEquals (trim.getGainDb(), InputTrimProcessor::kMaxDb);

            trim.setGainDb (-999.0f);
            expectEquals (trim.getGainDb(), InputTrimProcessor::kMinDb);

            trim.setGainDb (std::numeric_limits<float>::quiet_NaN());
            expectEquals (trim.getGainDb(), InputTrimProcessor::kMinDb,
                          "NaN overwrote a good value");

            trim.setGainDb (0.0f);
            expectEquals (trim.getGainDb(), 0.0f);
        }

        beginTest ("A change glides instead of stepping");
        {
            InputTrimProcessor trim;
            trim.setGainDb (0.0f);
            trim.prepareToPlay (kRate, 512, 1);

            juce::AudioBuffer<float> buffer (1, 512);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (0, i, 0.5f);

            trim.processBlock (buffer);
            const auto lastBefore = buffer.getSample (0, buffer.getNumSamples() - 1);

            trim.setGainDb (-24.0f);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (0, i, 0.5f);

            trim.processBlock (buffer);

            expect (std::abs (buffer.getSample (0, 0) - lastBefore) < 0.05f,
                    "trim stepped by " + juce::String (std::abs (buffer.getSample (0, 0) - lastBefore)));
        }

        beginTest ("Keeps the signal finite at the extremes");
        {
            for (const auto db : { InputTrimProcessor::kMinDb, 0.0f, InputTrimProcessor::kMaxDb })
            {
                InputTrimProcessor trim;
                trim.setGainDb (db);
                trim.prepareToPlay (kRate, 512, 2);

                juce::AudioBuffer<float> buffer (2, 512);
                fillSine (buffer, 82.0, 0.9f);
                trim.processBlock (buffer);

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < buffer.getNumSamples(); ++i)
                        expect (std::isfinite (buffer.getSample (ch, i)));
            }
        }

        beginTest ("Does nothing before prepareToPlay");
        {
            InputTrimProcessor trim;
            trim.setGainDb (12.0f);

            juce::AudioBuffer<float> buffer (1, 64);
            fillSine (buffer, 440.0, 0.1f);
            const auto before = peakOf (buffer);

            trim.processBlock (buffer);

            expectWithinAbsoluteError (peakOf (buffer), before, 1.0e-6f);
        }
    }
};

static InputTrimProcessorTests inputTrimProcessorTests;

//==============================================================================
/**
 * The reason the trim sits where it does.
 *
 * With the trim in front of the gate, the gate threshold stays correct relative
 * to the signal, so swapping guitars means re-dialling one knob. Behind the gate
 * the threshold would be tied to raw interface level and you would have to
 * re-dial both.
 */
class InputTrimOrderingTests final : public juce::UnitTest
{
public:
    InputTrimOrderingTests() : juce::UnitTest ("Input trim ordering", "dsp") {}

    void runTest() override
    {
        beginTest ("The trim is the first stage the guitar meets");
        {
            milodikfx::dsp::DSPChainManager manager;
            const auto chain = milodikfx::dsp::buildGuitarChain (manager);

            expect (chain.inputTrim != nullptr);
            expect (manager.findProcessor<milodikfx::dsp::InputTrimProcessor>() == chain.inputTrim);

            // The one-per-type rule still holds: the clean boost is a
            // GainProcessor and must remain the only one, or findProcessor
            // would start handing back the wrong stage.
            expect (manager.findProcessor<milodikfx::dsp::GainProcessor>() == chain.cleanBoost);
        }

        beginTest ("Trimming up is the same as a hotter guitar, as far as the gate is concerned");
        {
            // The load-bearing property. Both runs must gate identically:
            // quiet guitar + 12 dB trim, versus a guitar already 12 dB louder.
            const auto trimmed = runChain (0.05f, 12.0f);
            const auto hotter = runChain (0.05f * juce::Decibels::decibelsToGain (12.0f), 0.0f);

            expect (trimmed > 0.0f, "the trimmed run produced nothing at all");
            expectWithinAbsoluteError (trimmed, hotter, hotter * 0.02f,
                                       "the gate did not track the trim");
        }

        beginTest ("A signal only the trim lifts above the gate does get through");
        {
            // Without the trim in front, a quiet pickup is gated to silence and
            // no amount of downstream gain brings it back.
            const auto untrimmed = runChain (0.004f, 0.0f);
            const auto trimmed = runChain (0.004f, 24.0f);

            expect (untrimmed < 1.0e-3f, "the gate should have closed on this");
            expect (trimmed > untrimmed * 10.0f, "the trim did not open the gate");
        }
    }

private:
    /** Runs a sine through trim + gate and returns the settled output peak. */
    static float runChain (float amplitude, float trimDb)
    {
        milodikfx::dsp::InputTrimProcessor trim;
        milodikfx::dsp::NoiseGateProcessor gate;

        trim.setGainDb (trimDb);
        gate.setEnabled (true);
        gate.setThresholdDb (-40.0f);
        gate.setAttackMs (1.0f);
        gate.setHoldMs (10.0f);
        gate.setReleaseMs (20.0f);

        trim.prepareToPlay (kRate, 4096, 1);
        gate.prepareToPlay (kRate, 4096, 1);

        juce::AudioBuffer<float> buffer (1, 4096);

        for (int block = 0; block < 6; ++block)
        {
            fillSine (buffer, 220.0, amplitude, block * 4096);
            trim.processBlock (buffer);
            gate.processBlock (buffer);
        }

        return buffer.getMagnitude (2048, 2048);
    }
};

static InputTrimOrderingTests inputTrimOrderingTests;

//==============================================================================
class SpilloverTests final : public juce::UnitTest
{
public:
    SpilloverTests() : juce::UnitTest ("Delay and reverb spillover", "dsp") {}

    void runTest() override
    {
        using milodikfx::dsp::DelayProcessor;
        using milodikfx::dsp::ReverbProcessor;

        beginTest ("An enabled delay is bit-identical to spillover being off");
        {
            // Spillover only changes what happens on the way *out*. While the
            // effect is on, nothing about the signal may differ.
            juce::AudioBuffer<float> withSpill (2, 2048);
            juce::AudioBuffer<float> without (2, 2048);

            for (auto* pair : { &withSpill, &without })
                fillSine (*pair, 220.0, 0.4f);

            DelayProcessor a, b;

            for (auto* d : { &a, &b })
            {
                d->setEnabled (true);
                d->setTimeMs (120.0f);
                d->setFeedbackPercent (40.0f);
                d->setMixPercent (35.0f);
                d->prepareToPlay (kRate, 2048, 2);
            }

            a.setSpillover (true);
            b.setSpillover (false);

            a.processBlock (withSpill);
            b.processBlock (without);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < withSpill.getNumSamples(); ++i)
                    expect (withSpill.getSample (ch, i) == without.getSample (ch, i),
                            "spillover changed the sound while switched on, at sample "
                                + juce::String (i));
        }

        beginTest ("Switching the delay off lets the repeats ring on");
        {
            DelayProcessor delay;
            delay.setEnabled (true);
            delay.setSpillover (true);
            delay.setTimeMs (100.0f);
            delay.setFeedbackPercent (60.0f);
            delay.setMixPercent (100.0f);
            delay.prepareToPlay (kRate, 512, 1);

            // A burst, then switch off and feed silence.
            juce::AudioBuffer<float> burst (1, 4800);
            fillSine (burst, 440.0, 0.8f);
            delay.processBlock (burst);

            delay.setEnabled (false);

            juce::AudioBuffer<float> silence (1, 4800);
            silence.clear();
            delay.processBlock (silence);

            expect (silence.getMagnitude (0, silence.getNumSamples()) > 0.01f,
                    "the repeats were cut dead instead of ringing on");
            expect (delay.isTailRinging());
        }

        beginTest ("Without spillover the repeats stop dead, as before");
        {
            DelayProcessor delay;
            delay.setEnabled (true);
            delay.setSpillover (false);
            delay.setTimeMs (100.0f);
            delay.setFeedbackPercent (60.0f);
            delay.setMixPercent (100.0f);
            delay.prepareToPlay (kRate, 512, 1);

            juce::AudioBuffer<float> burst (1, 4800);
            fillSine (burst, 440.0, 0.8f);
            delay.processBlock (burst);

            delay.setEnabled (false);

            juce::AudioBuffer<float> silence (1, 4800);
            silence.clear();
            delay.processBlock (silence);

            expectEquals (silence.getMagnitude (0, silence.getNumSamples()), 0.0f);
        }

        beginTest ("The dry signal comes back to full when the delay is switched off");
        {
            // Turning the delay off must restore the unaffected signal, not
            // leave it attenuated by the mix it no longer applies.
            DelayProcessor delay;
            delay.setEnabled (true);
            delay.setSpillover (true);
            delay.setTimeMs (400.0f); // long, so no repeat lands in the window
            delay.setFeedbackPercent (0.0f);
            delay.setMixPercent (50.0f);
            delay.prepareToPlay (kRate, 4800, 1);

            juce::AudioBuffer<float> buffer (1, 4800);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (0, i, 0.5f);

            delay.processBlock (buffer);
            expectWithinAbsoluteError (buffer.getSample (0, 4000), 0.25f, 0.01f);

            delay.setEnabled (false);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (0, i, 0.5f);

            delay.processBlock (buffer);

            // Well past the 20 ms fade.
            expectWithinAbsoluteError (buffer.getSample (0, 4000), 0.5f, 0.01f);
        }

        beginTest ("The fade out does not step");
        {
            DelayProcessor delay;
            delay.setEnabled (true);
            delay.setSpillover (true);
            delay.setTimeMs (400.0f);
            delay.setFeedbackPercent (0.0f);
            delay.setMixPercent (50.0f);
            delay.prepareToPlay (kRate, 512, 1);

            juce::AudioBuffer<float> buffer (1, 512);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (0, i, 0.5f);

            delay.processBlock (buffer);
            const auto lastBefore = buffer.getSample (0, buffer.getNumSamples() - 1);

            delay.setEnabled (false);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (0, i, 0.5f);

            delay.processBlock (buffer);

            expect (std::abs (buffer.getSample (0, 0) - lastBefore) < 0.02f,
                    "switching off stepped by "
                        + juce::String (std::abs (buffer.getSample (0, 0) - lastBefore)));
        }

        beginTest ("A decayed delay stops processing entirely");
        {
            // Otherwise every switched-off delay would burn CPU forever ringing
            // silence around its feedback loop.
            DelayProcessor delay;
            delay.setEnabled (true);
            delay.setSpillover (true);
            delay.setTimeMs (50.0f);
            delay.setFeedbackPercent (10.0f);
            delay.setMixPercent (100.0f);
            delay.prepareToPlay (kRate, 4800, 1);

            juce::AudioBuffer<float> burst (1, 4800);
            fillSine (burst, 440.0, 0.5f);
            delay.processBlock (burst);

            delay.setEnabled (false);

            juce::AudioBuffer<float> silence (1, 4800);

            for (int block = 0; block < 40 && delay.isTailRinging(); ++block)
            {
                silence.clear();
                delay.processBlock (silence);
            }

            expect (! delay.isTailRinging(), "the tail never went idle");

            // And once idle it truly leaves the buffer alone.
            juce::AudioBuffer<float> probe (1, 512);

            for (int i = 0; i < probe.getNumSamples(); ++i)
                probe.setSample (0, i, 0.3f);

            delay.processBlock (probe);

            for (int i = 0; i < probe.getNumSamples(); ++i)
                expectEquals (probe.getSample (0, i), 0.3f);
        }

        beginTest ("Re-enabling after the tail has gone works immediately");
        {
            DelayProcessor delay;
            delay.setEnabled (false);
            delay.setSpillover (true);
            delay.setTimeMs (100.0f);
            delay.setFeedbackPercent (0.0f);
            delay.setMixPercent (100.0f);
            delay.prepareToPlay (kRate, 4800, 1);

            juce::AudioBuffer<float> idle (1, 4800);
            idle.clear();
            delay.processBlock (idle);

            delay.setEnabled (true);

            // An impulse now must appear one delay-time later, not be faded in.
            // Twice the delay time, so the repeat lands inside the buffer rather
            // than exactly one sample past its end.
            juce::AudioBuffer<float> buffer (1, 9600);
            buffer.clear();
            buffer.setSample (0, 0, 1.0f);

            delay.processBlock (buffer);

            auto loudest = 0.0f;
            auto loudestIndex = 0;

            for (int i = 100; i < buffer.getNumSamples(); ++i)
            {
                if (std::abs (buffer.getSample (0, i)) > loudest)
                {
                    loudest = std::abs (buffer.getSample (0, i));
                    loudestIndex = i;
                }
            }

            expect (loudest > 0.5f, "the repeat was faded in rather than instant");
            expect (std::abs ((double) loudestIndex / kRate - 0.1) < 0.03);
        }

        beginTest ("An enabled reverb is bit-identical to spillover being off");
        {
            juce::AudioBuffer<float> withSpill (2, 2048);
            juce::AudioBuffer<float> without (2, 2048);

            for (auto* pair : { &withSpill, &without })
                fillSine (*pair, 330.0, 0.4f);

            ReverbProcessor a, b;

            for (auto* r : { &a, &b })
            {
                r->setEnabled (true);
                r->setDryWetMix (0.4f);
                r->setDecayTime (3.0f);
                r->prepareToPlay (kRate, 2048, 2);
            }

            a.setSpillover (true);
            b.setSpillover (false);

            a.processBlock (withSpill);
            b.processBlock (without);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < withSpill.getNumSamples(); ++i)
                    expect (withSpill.getSample (ch, i) == without.getSample (ch, i),
                            "spillover changed the reverb while switched on");
        }

        beginTest ("Switching the reverb off lets the room ring on");
        {
            ReverbProcessor reverb;
            reverb.setEnabled (true);
            reverb.setSpillover (true);
            reverb.setDryWetMix (1.0f);
            reverb.setDecayTime (5.0f);
            reverb.prepareToPlay (kRate, 4800, 2);

            juce::AudioBuffer<float> burst (2, 9600);
            fillSine (burst, 440.0, 0.8f);
            reverb.processBlock (burst);

            reverb.setEnabled (false);

            juce::AudioBuffer<float> silence (2, 4800);
            silence.clear();
            reverb.processBlock (silence);

            expect (silence.getMagnitude (0, silence.getNumSamples()) > 0.001f,
                    "the room was cut dead instead of ringing on");
        }

        beginTest ("A decayed reverb stops processing entirely");
        {
            ReverbProcessor reverb;
            reverb.setEnabled (true);
            reverb.setSpillover (true);
            reverb.setDryWetMix (1.0f);
            reverb.setDecayTime (0.3f);
            reverb.prepareToPlay (kRate, 4800, 2);

            juce::AudioBuffer<float> burst (2, 4800);
            fillSine (burst, 440.0, 0.5f);
            reverb.processBlock (burst);

            reverb.setEnabled (false);

            juce::AudioBuffer<float> silence (2, 4800);

            for (int block = 0; block < 200 && reverb.isTailRinging(); ++block)
            {
                silence.clear();
                reverb.processBlock (silence);
            }

            expect (! reverb.isTailRinging(), "the reverb tail never went idle");

            juce::AudioBuffer<float> probe (2, 512);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < probe.getNumSamples(); ++i)
                    probe.setSample (ch, i, 0.3f);

            reverb.processBlock (probe);

            for (int i = 0; i < probe.getNumSamples(); ++i)
                expectEquals (probe.getSample (0, i), 0.3f);
        }
    }
};

static SpilloverTests spilloverTests;
