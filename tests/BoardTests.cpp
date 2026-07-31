#include <JuceHeader.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "dsp/ChainFactory.h"
#include "dsp/ChainOrder.h"
#include "dsp/DSPChainManager.h"
#include "preset/PresetManager.h"

namespace
{
constexpr double kRate = 48000.0;
constexpr int kBlock = 256;

using milodikfx::dsp::ChainOrder;
using milodikfx::dsp::DSPChainManager;

/** A stage that multiplies by a fixed gain, so its presence is unmistakable. */
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

void fillTone (juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample (ch, i, 0.3f * std::sin (juce::MathConstants<float>::twoPi * 220.0f
                                                      * (float) i / (float) kRate));
}

bool contains (const std::vector<std::string>& ids, const std::string& id)
{
    return std::find (ids.begin(), ids.end(), id) != ids.end();
}

/**
 * The board: which stages are in the chain at all.
 *
 * Distinct from bypass on purpose. A bypassed delay still runs so its spillover
 * tail keeps decaying; a delay that is off the board is not there, and the
 * result has to be bit-identical to a chain that never had one.
 */
class BoardTests final : public juce::UnitTest
{
public:
    BoardTests() : juce::UnitTest ("Board placement", "milodikfx") {}

    void runTest() override
    {
        beginTest ("Everything starts on the board");
        {
            DSPChainManager chain;
            const auto guitar = milodikfx::dsp::buildGuitarChain (chain);
            ChainOrder order (guitar, chain);

            // An install that has never heard of the board must behave exactly
            // as it always did, so the default is not "empty" -- it is "full".
            expectEquals ((int) order.getPlacedIds().size(), (int) order.getStageIds().size());
        }

        beginTest ("A stage off the board is skipped entirely");
        {
            DSPChainManager chain;
            chain.addProcessor (std::make_unique<GainMarker> (2.0f));
            chain.addProcessor (std::make_unique<GainMarker> (3.0f));
            chain.prepareToPlay (kRate, kBlock, 2);

            juce::AudioBuffer<float> both (2, kBlock);
            fillTone (both);
            chain.processBlock (both);

            chain.setStagePlaced (1, false);
            expect (! chain.isStagePlaced (1));

            juce::AudioBuffer<float> one (2, kBlock);
            fillTone (one);
            chain.processBlock (one);

            // 2*3 with both, 2 alone with the second off the board.
            expectWithinAbsoluteError (both.getSample (0, 10) / one.getSample (0, 10), 3.0f, 1.0e-4f);
        }

        beginTest ("An empty board is a straight wire, not silence");
        {
            // Fractal's grid crosses empty spaces with passive shunts, and this
            // is the same idea: nothing placed means the signal passes through
            // untouched. Silence would be the obvious wrong answer.
            DSPChainManager chain;
            chain.addProcessor (std::make_unique<GainMarker> (0.0f));
            chain.addProcessor (std::make_unique<GainMarker> (0.0f));
            chain.prepareToPlay (kRate, kBlock, 2);

            chain.setStagePlaced (0, false);
            chain.setStagePlaced (1, false);

            juce::AudioBuffer<float> buffer (2, kBlock);
            fillTone (buffer);

            juce::AudioBuffer<float> original (2, kBlock);
            original.makeCopyOf (buffer);

            chain.processBlock (buffer);

            for (int i = 0; i < kBlock; ++i)
                expectWithinAbsoluteError (buffer.getSample (0, i), original.getSample (0, i), 1.0e-6f);
        }

        beginTest ("Off the board is bit-identical to a chain built without it");
        {
            // The claim that matters: taking a stage off must not merely sound
            // similar to never having had it.
            const auto run = [] (bool includeExtra)
            {
                DSPChainManager chain;
                chain.addProcessor (std::make_unique<GainMarker> (0.7f));

                if (includeExtra)
                    chain.addProcessor (std::make_unique<GainMarker> (1.3f));

                chain.addProcessor (std::make_unique<GainMarker> (0.9f));
                chain.prepareToPlay (kRate, kBlock, 2);

                if (includeExtra)
                    chain.setStagePlaced (1, false);

                juce::AudioBuffer<float> buffer (2, kBlock);
                fillTone (buffer);
                chain.processBlock (buffer);

                return buffer;
            };

            const auto withStageOff = run (true);
            const auto neverBuilt = run (false);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < kBlock; ++i)
                    expect (withStageOff.getSample (ch, i) == neverBuilt.getSample (ch, i),
                            "taking a stage off the board is not bit-identical to omitting it");
        }

        beginTest ("The pinned stages cannot be taken off");
        {
            DSPChainManager chain;
            const auto guitar = milodikfx::dsp::buildGuitarChain (chain);
            ChainOrder order (guitar, chain);

            // The input meter reports inputDb + trimDb rather than measuring
            // twice, and the master carries the safety limiter. Neither is a
            // matter of taste, so neither is refusable at the API's discretion.
            order.setPlacedIds ({});

            const auto placed = order.getPlacedIds();

            expect (contains (placed, "input"), "the input trim came off the board");
            expect (contains (placed, "master"), "the master limiter came off the board");
            expect (! contains (placed, "overdrive"), "an ordinary stage survived an empty board");
        }

        beginTest ("Placement round-trips through ids");
        {
            DSPChainManager chain;
            const auto guitar = milodikfx::dsp::buildGuitarChain (chain);
            ChainOrder order (guitar, chain);

            order.setPlacedIds ({ "overdrive", "delay" });

            const auto placed = order.getPlacedIds();

            expect (contains (placed, "overdrive"));
            expect (contains (placed, "delay"));
            expect (! contains (placed, "reverb"));
            expect (! contains (placed, "cabinet"));

            order.placeAll();
            expectEquals ((int) order.getPlacedIds().size(), (int) order.getStageIds().size());
        }

        beginTest ("An unknown id in the list is ignored, not fatal");
        {
            // A preset outlives the build that wrote it, in both directions.
            DSPChainManager chain;
            const auto guitar = milodikfx::dsp::buildGuitarChain (chain);
            ChainOrder order (guitar, chain);

            order.setPlacedIds ({ "overdrive", "flanger-from-a-future-build" });

            const auto placed = order.getPlacedIds();

            expect (contains (placed, "overdrive"));
            expect (! contains (placed, "flanger-from-a-future-build"));
        }

        beginTest ("A stage off the board keeps its place in the order");
        {
            // Taking a block off and putting it back must not shuffle the rig:
            // the order is a separate value and placement does not touch it.
            DSPChainManager chain;
            const auto guitar = milodikfx::dsp::buildGuitarChain (chain);
            ChainOrder order (guitar, chain);

            const auto before = order.getIds();

            order.setPlacedIds ({ "overdrive" });
            expect (order.getIds() == before, "removing stages reordered the chain");

            order.placeAll();
            expect (order.getIds() == before, "putting stages back reordered the chain");
        }

        beginTest ("A preset with no board loads as a full board");
        {
            // The migration rule, stated as a test because getting it backwards
            // would silently empty every rig anyone has saved.
            expectEquals (milodikfx::preset::PresetManager::kSchemaVersion, 8);

            milodikfx::preset::PresetDocument document;
            expect (! document.chainBoard.isArray(),
                    "a fresh document must not claim to describe a board");
        }
    }
};

static BoardTests boardTests;
} // namespace
