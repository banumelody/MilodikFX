#include <JuceHeader.h>

#include <cmath>

#include "api/ParameterRegistry.h"
#include "dsp/ChainFactory.h"
#include "dsp/DSPChainManager.h"

namespace
{
/**
 * The rig this is built for: a Focusrite Scarlett over ASIO at 96 kHz with a
 * 32-sample buffer. That is a 0.33 ms budget per block, and it is the number
 * every realtime rule in this codebase exists to protect.
 */
constexpr double kRate = 96000.0;
constexpr int kBlock = 32;

/** Blocks per measurement. Enough to average out scheduler noise. */
constexpr int kBlocks = 4000;

void fillGuitarish (juce::AudioBuffer<float>& buffer, int blockIndex)
{
    // A note plus some harmonics, at a level a real pickup produces. Silence
    // would let the gate close and flatter every measurement.
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto n = (double) (blockIndex * buffer.getNumSamples() + i);
        const auto phase = juce::MathConstants<double>::twoPi * 220.0 * n / kRate;

        const auto s = (float) (0.25 * std::sin (phase)
                                + 0.08 * std::sin (phase * 2.0)
                                + 0.04 * std::sin (phase * 3.0));

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample (ch, i, s);
    }
}

/**
 * Runs the chain and returns the fraction of realtime it used.
 *
 * 1.0 means the block took exactly as long as it lasts, which is a dropout.
 */
double measureLoad (milodikfx::dsp::DSPChainManager& chain, int blocks = kBlocks)
{
    juce::AudioBuffer<float> buffer (2, kBlock);

    // A warm-up pass so first-touch page faults and branch predictors do not
    // land in the measurement.
    for (int i = 0; i < 200; ++i)
    {
        fillGuitarish (buffer, i);
        chain.processBlock (buffer);
    }

    const auto start = juce::Time::getHighResolutionTicks();

    for (int i = 0; i < blocks; ++i)
    {
        fillGuitarish (buffer, i);
        chain.processBlock (buffer);
    }

    const auto elapsed = juce::Time::highResolutionTicksToSeconds (
        juce::Time::getHighResolutionTicks() - start);

    const auto audioSeconds = (double) (blocks * kBlock) / kRate;

    return elapsed / audioSeconds;
}

/** Switches every effect on, so nothing is skipped by an early return. */
void enableEverything (const milodikfx::api::ParameterRegistry& registry)
{
    for (const auto& effect : registry.getEffects())
        if (effect.setEnabled)
            effect.setEnabled (true);
}
} // namespace

//==============================================================================
/**
 * These assert generously on purpose.
 *
 * The test binary is built Debug, where the DSP runs several times slower than
 * the Release build that actually gets played through, so a tight bound here
 * would fail for reasons that have nothing to do with the code. What the
 * numbers are for is the log: a change that doubles the cost shows up
 * immediately even though the absolute figure is not the shipping one. The
 * ratio tests below are build-independent and are the ones that bite.
 */
class PerformanceTests final : public juce::UnitTest
{
public:
    PerformanceTests() : juce::UnitTest ("Performance", "perf") {}

    void runTest() override
    {
        beginTest ("The whole chain keeps up with realtime at 32 samples / 96 kHz");
        {
            milodikfx::dsp::DSPChainManager chain;
            const auto processors = milodikfx::dsp::buildGuitarChain (chain);

            milodikfx::api::ParameterRegistry registry;
            milodikfx::dsp::registerChainParameters (registry, processors, chain);

            enableEverything (registry);
            chain.prepareToPlay (kRate, kBlock, 2);

            const auto load = measureLoad (chain);

            logMessage ("  everything on, Custom drive: "
                        + juce::String (load * 100.0, 1) + " % of realtime (Debug build)");

            expect (load < 1.0,
                    "the chain cannot keep up even in a Debug build: "
                        + juce::String (load * 100.0, 1) + " %");
        }

        beginTest ("The most expensive drive voicing still keeps up");
        {
            // Every voicing filter running, at the highest oversampling factor,
            // with a cascaded two-stage curve. This is the worst case a user
            // can dial in.
            milodikfx::dsp::DSPChainManager chain;
            const auto processors = milodikfx::dsp::buildGuitarChain (chain);

            milodikfx::api::ParameterRegistry registry;
            milodikfx::dsp::registerChainParameters (registry, processors, chain);

            enableEverything (registry);

            processors.overdrive->setType (milodikfx::dsp::drive::marshallInABox);
            processors.overdrive->setDrivePercent (90.0f);
            processors.overdrive->setOversamplingIndex (3); // 8x

            chain.prepareToPlay (kRate, kBlock, 2);

            const auto load = measureLoad (chain);

            logMessage ("  worst case, MIAB at 8x: "
                        + juce::String (load * 100.0, 1) + " % of realtime (Debug build)");

            expect (load < 1.0,
                    "the worst case cannot keep up even in a Debug build: "
                        + juce::String (load * 100.0, 1) + " %");
        }

        beginTest ("Every voicing costs roughly the same");
        {
            // A voicing that is quietly ten times more expensive than its
            // neighbours would be a trap: you would dial it in at home and find
            // out on stage.
            double cheapest = 1.0e9;
            double dearest = 0.0;

            for (int type = 0; type < milodikfx::dsp::drive::numTypes; ++type)
            {
                milodikfx::dsp::DSPChainManager chain;
                const auto processors = milodikfx::dsp::buildGuitarChain (chain);

                milodikfx::api::ParameterRegistry registry;
                milodikfx::dsp::registerChainParameters (registry, processors, chain);

                enableEverything (registry);
                processors.overdrive->setType (type);
                processors.overdrive->setDrivePercent (70.0f);
                processors.overdrive->setOversamplingIndex (1);

                chain.prepareToPlay (kRate, kBlock, 2);

                const auto load = measureLoad (chain, 1500);

                logMessage ("  type " + juce::String (type) + ": "
                            + juce::String (load * 100.0, 1) + " %");

                cheapest = juce::jmin (cheapest, load);
                dearest = juce::jmax (dearest, load);
            }

            expect (dearest < cheapest * 3.0,
                    "one voicing is far more expensive than the rest: "
                        + juce::String (dearest / cheapest, 2) + "x");
        }

        beginTest ("A decayed spillover tail really does cost nothing");
        {
            // The whole point of the idle check. This ratio is
            // build-independent, so it is a real assertion rather than a
            // Debug-flattered one.
            milodikfx::dsp::DSPChainManager chain;
            const auto processors = milodikfx::dsp::buildGuitarChain (chain);

            milodikfx::api::ParameterRegistry registry;
            milodikfx::dsp::registerChainParameters (registry, processors, chain);

            enableEverything (registry);
            processors.delay->setMixPercent (100.0f);
            processors.reverb->setDecayTime (0.3f);
            chain.prepareToPlay (kRate, kBlock, 2);

            const auto ringing = measureLoad (chain, 2000);

            // Switch both off and let the tails decay away.
            processors.delay->setEnabled (false);
            processors.reverb->setEnabled (false);

            juce::AudioBuffer<float> silence (2, kBlock);

            for (int i = 0; i < 8000; ++i)
            {
                silence.clear();
                chain.processBlock (silence);
            }

            expect (! processors.delay->isTailRinging(), "the delay tail never went idle");
            expect (! processors.reverb->isTailRinging(), "the reverb tail never went idle");

            const auto idle = measureLoad (chain, 2000);

            logMessage ("  ringing " + juce::String (ringing * 100.0, 1)
                        + " % vs idle " + juce::String (idle * 100.0, 1) + " %");

            expect (idle < ringing,
                    "a decayed tail cost as much as a ringing one: idle "
                        + juce::String (idle * 100.0, 1) + " % vs "
                        + juce::String (ringing * 100.0, 1) + " %");
        }

        beginTest ("The input trim is free at its default");
        {
            // It is first in the chain and always in the path, so if it cost
            // anything at 0 dB it would cost that on every single block.
            milodikfx::dsp::DSPChainManager chain;
            const auto processors = milodikfx::dsp::buildGuitarChain (chain);

            milodikfx::api::ParameterRegistry registry;
            milodikfx::dsp::registerChainParameters (registry, processors, chain);

            chain.prepareToPlay (kRate, kBlock, 2);

            processors.inputTrim->setGainDb (0.0f);
            const auto atUnity = measureLoad (chain, 2000);

            processors.inputTrim->setGainDb (6.0f);
            const auto atGain = measureLoad (chain, 2000);

            logMessage ("  trim at 0 dB " + juce::String (atUnity * 100.0, 2)
                        + " % vs +6 dB " + juce::String (atGain * 100.0, 2) + " %");

            expect (atUnity <= atGain * 1.5,
                    "the unity path is not cheaper than the gain path");
        }

        beginTest ("Cost does not creep block to block");
        {
            // A processor that quietly grew per-block state -- a vector that
            // reallocated, a filter that never settled -- would show up as a
            // second half slower than the first.
            milodikfx::dsp::DSPChainManager chain;
            const auto processors = milodikfx::dsp::buildGuitarChain (chain);

            milodikfx::api::ParameterRegistry registry;
            milodikfx::dsp::registerChainParameters (registry, processors, chain);

            enableEverything (registry);
            processors.overdrive->setType (milodikfx::dsp::drive::tubeScreamer);
            processors.overdrive->setDrivePercent (70.0f);
            chain.prepareToPlay (kRate, kBlock, 2);

            const auto first = measureLoad (chain, 3000);
            const auto second = measureLoad (chain, 3000);

            logMessage ("  first " + juce::String (first * 100.0, 1)
                        + " % then " + juce::String (second * 100.0, 1) + " %");

            expect (second < first * 1.5,
                    "the chain got measurably slower as it ran: "
                        + juce::String (second / first, 2) + "x");
        }

        beginTest ("Nothing in the chain produces a non-finite sample under load");
        {
            // Denormals are the classic way a decaying tail silently costs
            // hundreds of times more; ScopedNoDenormals is what prevents it,
            // and a NaN would be worse still.
            milodikfx::dsp::DSPChainManager chain;
            const auto processors = milodikfx::dsp::buildGuitarChain (chain);

            milodikfx::api::ParameterRegistry registry;
            milodikfx::dsp::registerChainParameters (registry, processors, chain);

            enableEverything (registry);
            processors.overdrive->setDrivePercent (100.0f);
            processors.delay->setFeedbackPercent (95.0f);
            processors.reverb->setDecayTime (10.0f);
            chain.prepareToPlay (kRate, kBlock, 2);

            juce::AudioBuffer<float> buffer (2, kBlock);

            for (int block = 0; block < 3000; ++block)
            {
                // Loud for a while, then silence, which is where a decaying
                // tail drifts into denormal territory.
                if (block < 500)
                    fillGuitarish (buffer, block);
                else
                    buffer.clear();

                chain.processBlock (buffer);

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < kBlock; ++i)
                        expect (std::isfinite (buffer.getSample (ch, i)),
                                "non-finite sample in block " + juce::String (block));
            }
        }
    }
};

static PerformanceTests performanceTests;
