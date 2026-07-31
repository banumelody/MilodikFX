#include <JuceHeader.h>

#include <cmath>
#include <string>
#include <vector>

#include "api/ParameterRegistry.h"
#include "dsp/ChainFactory.h"
#include "dsp/ChainOrder.h"
#include "dsp/DSPChainManager.h"
#include "dsp/MixerProcessor.h"
#include "dsp/SplitProcessor.h"

namespace
{
constexpr double kRate = 48000.0;
constexpr int kBlock = 512;

/** A stage that multiplies by a fixed gain, so a path can be identified by ear. */
class GainMarker final : public milodikfx::dsp::AudioProcessorBase
{
public:
    explicit GainMarker (float gainToUse) : gain (gainToUse) {}

    void prepareToPlay (double, int, int) override {}
    void reset() override {}

    void processBlock (juce::AudioBuffer<float>& buffer) override
    {
        buffer.applyGain (gain);
    }

private:
    float gain;
};

void fillTone (juce::AudioBuffer<float>& b, double increment = 0.05)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (ch, i, 0.3f * (float) std::sin (increment * (double) i));
}

/**
 * A sine that carries its phase across blocks.
 *
 * Refilling from phase zero every block puts a step at each boundary, and a step
 * is broadband: measuring a crossover that way shows 80 Hz pouring into the high
 * path and the two halves summing 4 dB hot. That is the measurement being wrong,
 * not the filter -- so the phase has to run on.
 */
struct ContinuousTone
{
    double phase = 0.0;

    void fill (juce::AudioBuffer<float>& b, double freqHz, double sampleRate)
    {
        const auto increment = 2.0 * juce::MathConstants<double>::pi * freqHz / sampleRate;

        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const auto s = 0.3f * (float) std::sin (phase);
            phase += increment;

            if (phase > 2.0 * juce::MathConstants<double>::pi)
                phase -= 2.0 * juce::MathConstants<double>::pi;

            for (int ch = 0; ch < b.getNumChannels(); ++ch)
                b.setSample (ch, i, s);
        }
    }
};

class SplitEngineTests final : public juce::UnitTest
{
public:
    SplitEngineTests() : juce::UnitTest ("Split A/B", "dsp") {}

    void runTest() override
    {
        using milodikfx::dsp::MixerProcessor;
        using milodikfx::dsp::SplitProcessor;

        beginTest ("A disabled split leaves the chain bit-identical");
        {
            // The whole backwards-compatibility promise. Every preset anyone has
            // dialled in predates the split, and switching it off must give back
            // exactly the samples the serial chain always produced -- not
            // approximately, exactly.
            const auto run = [] (bool withSplitStages)
            {
                milodikfx::dsp::DSPChainManager chain;

                chain.addProcessor (std::make_unique<GainMarker> (0.9f));

                milodikfx::dsp::SplitProcessor* split = nullptr;
                milodikfx::dsp::MixerProcessor* mixer = nullptr;

                if (withSplitStages)
                {
                    split = dynamic_cast<SplitProcessor*> (
                        chain.addProcessor (std::make_unique<SplitProcessor>()));
                }

                chain.addProcessor (std::make_unique<GainMarker> (0.8f));

                if (withSplitStages)
                {
                    mixer = dynamic_cast<MixerProcessor*> (
                        chain.addProcessor (std::make_unique<MixerProcessor>()));
                    chain.setParallelStages (split, mixer);
                }

                chain.prepareToPlay (kRate, kBlock, 2);

                juce::AudioBuffer<float> buffer (2, kBlock);
                fillTone (buffer);
                chain.processBlock (buffer);

                return buffer;
            };

            const auto serial = run (false);
            const auto withStages = run (true);

            auto identical = true;

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < kBlock; ++i)
                    if (serial.getSample (ch, i) != withStages.getSample (ch, i))
                        identical = false;

            expect (identical, "an off split changed the signal");
        }

        beginTest ("An enabled split with everything centred matches the serial chain");
        {
            // Both paths dead centre, both levels at unity: the parallel section
            // should sound like no section at all. Without the constant-power
            // compensation in the mixer this comes out 3 dB down, which is
            // exactly the sort of thing nobody notices until they A/B it.
            milodikfx::dsp::DSPChainManager chain;

            auto* split = dynamic_cast<SplitProcessor*> (
                chain.addProcessor (std::make_unique<SplitProcessor>()));
            chain.addProcessor (std::make_unique<GainMarker> (1.0f));
            auto* mixer = dynamic_cast<MixerProcessor*> (
                chain.addProcessor (std::make_unique<MixerProcessor>()));

            chain.setParallelStages (split, mixer);
            chain.prepareToPlay (kRate, kBlock, 2);

            // Path A alone carries the signal; B is silent because the only
            // stage is on A and B gets the same input then nothing done to it.
            // So set B's level to zero to isolate A's gain staging.
            mixer->setLevelB (0.0f);
            split->setEnabled (true);

            juce::AudioBuffer<float> buffer (2, kBlock);
            fillTone (buffer);

            juce::AudioBuffer<float> original (2, kBlock);
            original.makeCopyOf (buffer);

            chain.processBlock (buffer);

            for (int i = 0; i < kBlock; ++i)
                expectWithinAbsoluteError (buffer.getSample (0, i), original.getSample (0, i), 1.0e-5f);
        }

        beginTest ("A stage assigned to path B runs on B, not on A");
        {
            milodikfx::dsp::DSPChainManager chain;

            auto* split = dynamic_cast<SplitProcessor*> (
                chain.addProcessor (std::make_unique<SplitProcessor>()));

            // index 1 stays on A, index 2 goes to B.
            chain.addProcessor (std::make_unique<GainMarker> (1.0f));   // A: unchanged
            chain.addProcessor (std::make_unique<GainMarker> (0.0f));   // B: silenced

            auto* mixer = dynamic_cast<MixerProcessor*> (
                chain.addProcessor (std::make_unique<MixerProcessor>()));

            chain.setParallelStages (split, mixer);
            chain.prepareToPlay (kRate, kBlock, 2);

            split->setEnabled (true);
            chain.setStageOnBusB (2, true);

            expect (chain.isStageOnBusB (2));
            expect (! chain.isStageOnBusB (1));

            juce::AudioBuffer<float> buffer (2, kBlock);
            fillTone (buffer);
            chain.processBlock (buffer);

            // B was silenced, so only A's copy survives -- at the centred
            // constant-power gain, which is unity after compensation.
            const auto peak = buffer.getMagnitude (0, 0, kBlock);
            expect (peak > 0.2f, "path A vanished as well");

            // And now the other way round: silence A instead.
            chain.setStageOnBusB (2, false);
            chain.setStageOnBusB (1, true);

            juce::AudioBuffer<float> second (2, kBlock);
            fillTone (second);
            chain.processBlock (second);

            expect (second.getMagnitude (0, 0, kBlock) > 0.2f, "path B vanished");
        }

        beginTest ("Pan sends each path to its own side");
        {
            milodikfx::dsp::DSPChainManager chain;

            auto* split = dynamic_cast<SplitProcessor*> (
                chain.addProcessor (std::make_unique<SplitProcessor>()));
            chain.addProcessor (std::make_unique<GainMarker> (1.0f));
            chain.addProcessor (std::make_unique<GainMarker> (0.5f));
            auto* mixer = dynamic_cast<MixerProcessor*> (
                chain.addProcessor (std::make_unique<MixerProcessor>()));

            chain.setParallelStages (split, mixer);
            chain.prepareToPlay (kRate, kBlock, 2);

            split->setEnabled (true);
            chain.setStageOnBusB (2, true);

            // A hard left, B hard right: the stereo rig everyone actually wants.
            mixer->setPanA (-1.0f);
            mixer->setPanB (1.0f);

            juce::AudioBuffer<float> buffer (2, kBlock);
            fillTone (buffer);
            chain.processBlock (buffer);

            const auto left = buffer.getMagnitude (0, 0, kBlock);
            const auto right = buffer.getMagnitude (1, 0, kBlock);

            logMessage ("  hard-panned: left " + juce::String (left, 4)
                        + " right " + juce::String (right, 4));

            // B is half the level of A, so the sides must differ substantially --
            // if the pan did nothing they would be equal.
            expect (left > 0.1f && right > 0.05f, "a side went silent");
            expect (std::abs (left - right) > 0.05f, "the two paths are not being panned apart");
        }

        beginTest ("The crossover splits low from high and sums back flat");
        {
            milodikfx::dsp::SplitProcessor split;
            split.prepareToPlay (kRate, kBlock, 2);
            split.setEnabled (true);
            split.setMode (SplitProcessor::Mode::crossover);
            split.setFrequencyHz (500.0f);

            const auto energyAt = [&] (double freqHz, int channel)
            {
                juce::AudioBuffer<float> a (2, kBlock);
                juce::AudioBuffer<float> b (2, kBlock);
                b.clear();

                split.reset();
                ContinuousTone tone;

                // Settle the filters, then measure the block after.
                for (int pass = 0; pass < 24; ++pass)
                {
                    tone.fill (a, freqHz, kRate);
                    split.divide (a, b);
                }

                return channel == 0 ? a.getMagnitude (0, 0, kBlock) : b.getMagnitude (0, 0, kBlock);
            };

            const auto lowIntoA = energyAt (80.0, 0);
            const auto lowIntoB = energyAt (80.0, 1);
            const auto highIntoA = energyAt (4000.0, 0);
            const auto highIntoB = energyAt (4000.0, 1);

            logMessage ("  80 Hz -> A " + juce::String (lowIntoA, 4) + " / B " + juce::String (lowIntoB, 4));
            logMessage ("  4 kHz -> A " + juce::String (highIntoA, 4) + " / B " + juce::String (highIntoB, 4));

            expect (lowIntoA > lowIntoB * 4.0f, "low frequencies did not go to path A");
            expect (highIntoB > highIntoA * 4.0f, "high frequencies did not go to path B");
        }

        beginTest ("A crossover sums back to roughly flat");
        {
            // Linkwitz-Riley exists precisely so the two halves add back up. A
            // single Butterworth section per side would leave a bump right at
            // the crossover, which on a bass rig is where it would be heard.
            milodikfx::dsp::SplitProcessor split;
            split.prepareToPlay (kRate, kBlock, 2);
            split.setEnabled (true);
            split.setMode (SplitProcessor::Mode::crossover);
            split.setFrequencyHz (500.0f);

            const auto sumAt = [&] (double freqHz)
            {
                juce::AudioBuffer<float> a (2, kBlock);
                juce::AudioBuffer<float> b (2, kBlock);
                b.clear();

                split.reset();
                ContinuousTone tone;

                juce::AudioBuffer<float> summed (2, kBlock);

                for (int pass = 0; pass < 24; ++pass)
                {
                    tone.fill (a, freqHz, kRate);
                    split.divide (a, b);

                    for (int i = 0; i < kBlock; ++i)
                        summed.setSample (0, i, a.getSample (0, i) + b.getSample (0, i));
                }

                return summed.getMagnitude (0, 0, kBlock);
            };

            // Well below, at, and well above the crossover. 0.3 is the input peak.
            const auto below = sumAt (100.0);
            const auto at = sumAt (500.0);
            const auto above = sumAt (3000.0);

            logMessage ("  summed: 100 Hz " + juce::String (below, 4)
                        + " / 500 Hz " + juce::String (at, 4)
                        + " / 3 kHz " + juce::String (above, 4));

            // Within about 1.5 dB of the input across the range. An LR4 sum is
            // magnitude-flat; the slack is for the settling window.
            for (const auto measured : { below, at, above })
                expect (measured > 0.24f && measured < 0.37f,
                        "the crossover does not sum back flat: " + juce::String (measured, 4));
        }

        beginTest ("A mixer dragged in front of its split still folds path B back");
        {
            // Nothing stops someone dragging the brackets out of order. The
            // signal on B must not silently disappear when they do.
            milodikfx::dsp::DSPChainManager chain;

            auto* split = dynamic_cast<SplitProcessor*> (
                chain.addProcessor (std::make_unique<SplitProcessor>()));
            auto* mixer = dynamic_cast<MixerProcessor*> (
                chain.addProcessor (std::make_unique<MixerProcessor>()));
            chain.addProcessor (std::make_unique<GainMarker> (1.0f));

            chain.setParallelStages (split, mixer);
            chain.prepareToPlay (kRate, kBlock, 2);
            split->setEnabled (true);

            // Mixer first, then split: reversed.
            expect (chain.setOrder ({ 1, 0, 2 }));

            juce::AudioBuffer<float> buffer (2, kBlock);
            fillTone (buffer);
            chain.processBlock (buffer);

            auto allFinite = true;
            for (int i = 0; i < kBlock; ++i)
                if (! std::isfinite (buffer.getSample (0, i)))
                    allFinite = false;

            expect (allFinite, "a reversed split/mixer pair produced non-finite samples");
            expect (buffer.getMagnitude (0, 0, kBlock) > 0.05f, "the signal was lost entirely");
        }

        beginTest ("L/R mode routes each input channel to its own path");
        {
            // The two-pickup case: a magnetic and a piezo arrive as two
            // independent signals, and each must reach its own chain intact.
            // Marker gains rather than a tone, so which path a sample came down
            // is readable from its value alone.
            milodikfx::dsp::DSPChainManager chain;

            auto* split = dynamic_cast<SplitProcessor*> (
                chain.addProcessor (std::make_unique<SplitProcessor>()));

            chain.addProcessor (std::make_unique<GainMarker> (2.0f));   // A
            chain.addProcessor (std::make_unique<GainMarker> (4.0f));   // B

            auto* mixer = dynamic_cast<MixerProcessor*> (
                chain.addProcessor (std::make_unique<MixerProcessor>()));

            chain.setParallelStages (split, mixer);
            chain.prepareToPlay (kRate, kBlock, 2);

            split->setEnabled (true);
            split->setMode (SplitProcessor::Mode::leftRight);
            chain.setStageOnBusB (2, true);

            // Both paths centred, deliberately. Hard-panning them apart makes
            // this test pass in `even` mode too -- A's left and B's right carry
            // the same samples either way -- and a test that cannot fail is
            // worse than no test, because it reads as evidence.
            juce::AudioBuffer<float> buffer (2, kBlock);

            for (int i = 0; i < kBlock; ++i)
            {
                buffer.setSample (0, i, 0.10f);   // "magnetic" on the left
                buffer.setSample (1, i, 0.01f);   // "piezo" on the right
            }

            chain.processBlock (buffer);

            // Each source is duplicated across its own path's two channels, so
            // the mix is (left * 2) + (right * 4) and both outputs carry it
            // equally. Centred constant-power gain times the sqrt(2)
            // compensation is exactly unity.
            constexpr auto expected = 0.10f * 2.0f + 0.01f * 4.0f;

            expectWithinAbsoluteError (buffer.getSample (0, 0), expected, 1.0e-4f);
            expectWithinAbsoluteError (buffer.getSample (1, 0), expected, 1.0e-4f);

            // And the same rig in `even` mode must land somewhere else, or the
            // assertions above are measuring something other than the routing.
            split->setMode (SplitProcessor::Mode::even);

            juce::AudioBuffer<float> evenRun (2, kBlock);

            for (int i = 0; i < kBlock; ++i)
            {
                evenRun.setSample (0, i, 0.10f);
                evenRun.setSample (1, i, 0.01f);
            }

            chain.processBlock (evenRun);

            expectWithinAbsoluteError (evenRun.getSample (0, 0), 0.10f * 2.0f + 0.10f * 4.0f, 1.0e-4f);
            expect (std::abs (evenRun.getSample (0, 0) - buffer.getSample (0, 0)) > 0.1f,
                    "L/R and even produced the same output; the mode is not being applied");
        }

        beginTest ("L/R mode with identical channels is the same as even mode");
        {
            // A mono rig feeds both channels the same thing, and then the two
            // modes describe the same routing -- so they must not disagree.
            const auto run = [] (SplitProcessor::Mode mode)
            {
                milodikfx::dsp::DSPChainManager chain;

                auto* split = dynamic_cast<SplitProcessor*> (
                    chain.addProcessor (std::make_unique<SplitProcessor>()));
                chain.addProcessor (std::make_unique<GainMarker> (1.0f));
                chain.addProcessor (std::make_unique<GainMarker> (0.5f));
                auto* mixer = dynamic_cast<MixerProcessor*> (
                    chain.addProcessor (std::make_unique<MixerProcessor>()));

                chain.setParallelStages (split, mixer);
                chain.prepareToPlay (kRate, kBlock, 2);
                split->setEnabled (true);
                split->setMode (mode);
                chain.setStageOnBusB (2, true);

                juce::AudioBuffer<float> buffer (2, kBlock);
                fillTone (buffer);
                buffer.copyFrom (1, 0, buffer, 0, 0, kBlock);   // mono: L == R

                chain.processBlock (buffer);
                return buffer;
            };

            const auto even = run (SplitProcessor::Mode::even);
            const auto leftRight = run (SplitProcessor::Mode::leftRight);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < kBlock; ++i)
                    expectWithinAbsoluteError (leftRight.getSample (ch, i), even.getSample (ch, i), 1.0e-6f);
        }

        beginTest ("Inverting path B cancels a duplicate instead of doubling it");
        {
            // What the control is for: two pickups sensing the same string can
            // partially cancel when blended, and the fix is polarity, not delay.
            milodikfx::dsp::DSPChainManager chain;

            auto* split = dynamic_cast<SplitProcessor*> (
                chain.addProcessor (std::make_unique<SplitProcessor>()));
            chain.addProcessor (std::make_unique<GainMarker> (1.0f));
            auto* mixer = dynamic_cast<MixerProcessor*> (
                chain.addProcessor (std::make_unique<MixerProcessor>()));

            chain.setParallelStages (split, mixer);
            chain.prepareToPlay (kRate, kBlock, 2);
            split->setEnabled (true);   // even mode: both paths carry the same

            expect (! mixer->getInvertB(), "invert must default to off");

            juce::AudioBuffer<float> summed (2, kBlock);
            fillTone (summed);
            chain.processBlock (summed);
            expect (summed.getMagnitude (0, 0, kBlock) > 0.1f, "the sum vanished");

            mixer->setInvertB (true);

            juce::AudioBuffer<float> cancelled (2, kBlock);
            fillTone (cancelled);
            chain.processBlock (cancelled);

            for (int i = 0; i < kBlock; ++i)
                expectWithinAbsoluteError (cancelled.getSample (0, i), 0.0f, 1.0e-6f);
        }

        beginTest ("The real chain exposes the split and the mixer as stages");
        {
            milodikfx::dsp::DSPChainManager manager;
            const auto chain = milodikfx::dsp::buildGuitarChain (manager);
            milodikfx::dsp::ChainOrder order (chain, manager);

            const auto ids = order.getIds();

            expect (std::find (ids.begin(), ids.end(), "split") != ids.end());
            expect (std::find (ids.begin(), ids.end(), "mixer") != ids.end());

            // Neither is pinned: where the parallel section sits is the whole
            // point of being able to drag them.
            expect (! order.isFixed ("split"));
            expect (! order.isFixed ("mixer"));

            // And the split must be off out of the box.
            expect (! chain.split->isEnabled());
        }

        beginTest ("The registry exposes all three split modes and the invert");
        {
            milodikfx::dsp::DSPChainManager manager;
            const auto chain = milodikfx::dsp::buildGuitarChain (manager);

            milodikfx::api::ParameterRegistry registry;
            milodikfx::dsp::registerChainParameters (registry, chain, manager);

            const auto* mode = registry.findParameter ("split", "mode");
            expect (mode != nullptr, "split.mode is missing");

            // The range has to reach 2 or L/R can never be selected -- and a
            // clamped-away mode is invisible rather than merely broken.
            expectWithinAbsoluteError (mode->maxValue, 2.0f, 1.0e-6f);
            expectWithinAbsoluteError (mode->defaultValue, 0.0f, 1.0e-6f);

            mode->set (2.0f);
            expect (chain.split->getMode() == SplitProcessor::Mode::leftRight);
            expectWithinAbsoluteError (mode->get(), 2.0f, 1.0e-6f);

            // Out of range must land somewhere real rather than on a mode that
            // does not exist: the setter clamps rather than casting blindly.
            mode->set (9.0f);
            expect (chain.split->getMode() == SplitProcessor::Mode::leftRight);

            mode->set (0.0f);
            expect (chain.split->getMode() == SplitProcessor::Mode::even);

            const auto* invert = registry.findParameter ("mixer", "invertB");
            expect (invert != nullptr, "mixer.invertB is missing");
            expectWithinAbsoluteError (invert->defaultValue, 0.0f, 1.0e-6f);

            invert->set (1.0f);
            expect (chain.mixer->getInvertB());
            invert->set (0.0f);
            expect (! chain.mixer->getInvertB());
        }
    }
};

static SplitEngineTests splitEngineTests;
} // namespace
