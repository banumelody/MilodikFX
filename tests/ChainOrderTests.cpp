#include <JuceHeader.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "dsp/ChainFactory.h"
#include "dsp/DSPChainManager.h"

namespace
{
/**
 * A stage that records the order it ran in.
 *
 * Each one writes its own letter into a shared string as it processes, so a
 * block's worth of processing leaves behind the exact sequence the chain ran --
 * which is the only way to assert on run order rather than on what the manager
 * says the order is.
 */
class MarkerProcessor final : public milodikfx::dsp::AudioProcessorBase
{
public:
    MarkerProcessor (char letterToUse, std::string& logToUse)
        : letter (letterToUse), log (logToUse)
    {
    }

    void prepareToPlay (double, int, int) override {}
    void reset() override {}

    void processBlock (juce::AudioBuffer<float>&) override { log.push_back (letter); }

private:
    char letter;
    std::string& log;
};

class ChainOrderTests final : public juce::UnitTest
{
public:
    ChainOrderTests() : juce::UnitTest ("Chain order", "dsp") {}

    void runTest() override
    {
        beginTest ("A freshly built chain runs in the order it was built");
        {
            std::string log;
            milodikfx::dsp::DSPChainManager chain;

            for (char c : { 'a', 'b', 'c', 'd' })
                chain.addProcessor (std::make_unique<MarkerProcessor> (c, log));

            chain.prepareToPlay (48000.0, 64, 2);

            juce::AudioBuffer<float> buffer (2, 64);
            buffer.clear();
            chain.processBlock (buffer);

            expectEquals (juce::String (log), juce::String ("abcd"));
            expectEquals ((int) chain.getOrder().size(), 4);
        }

        beginTest ("A reordered chain actually runs in the new order");
        {
            std::string log;
            milodikfx::dsp::DSPChainManager chain;

            for (char c : { 'a', 'b', 'c', 'd' })
                chain.addProcessor (std::make_unique<MarkerProcessor> (c, log));

            chain.prepareToPlay (48000.0, 64, 2);

            expect (chain.setOrder ({ 2, 0, 3, 1 }), "a valid permutation was refused");

            juce::AudioBuffer<float> buffer (2, 64);
            buffer.clear();
            log.clear();
            chain.processBlock (buffer);

            expectEquals (juce::String (log), juce::String ("cadb"));
        }

        beginTest ("An invalid order is refused rather than half-applied");
        {
            std::string log;
            milodikfx::dsp::DSPChainManager chain;

            for (char c : { 'a', 'b', 'c', 'd' })
                chain.addProcessor (std::make_unique<MarkerProcessor> (c, log));

            chain.prepareToPlay (48000.0, 64, 2);
            expect (chain.setOrder ({ 3, 2, 1, 0 }));

            // Each of these would run one stage twice and drop another, or run a
            // stage that does not exist.
            expect (! chain.setOrder ({ 0, 0, 1, 2 }), "a duplicate index was accepted");
            expect (! chain.setOrder ({ 0, 1, 2 }), "a short order was accepted");
            expect (! chain.setOrder ({ 0, 1, 2, 3, 3 }), "a long order was accepted");
            expect (! chain.setOrder ({ 0, 1, 2, 9 }), "an out-of-range index was accepted");
            expect (! chain.setOrder ({ -1, 1, 2, 3 }), "a negative index was accepted");

            // The order that was in force must be exactly what it was.
            juce::AudioBuffer<float> buffer (2, 64);
            buffer.clear();
            log.clear();
            chain.processBlock (buffer);

            expectEquals (juce::String (log), juce::String ("dcba"),
                          "a refused order still changed what runs");
        }

        beginTest ("Pinned stages cannot be moved");
        {
            std::string log;
            milodikfx::dsp::DSPChainManager chain;

            for (char c : { 'a', 'b', 'c', 'd' })
                chain.addProcessor (std::make_unique<MarkerProcessor> (c, log));

            chain.prepareToPlay (48000.0, 64, 2);

            // First and last fixed: the input trim and the master stage. The
            // master carries the safety limiter, so nothing may follow it.
            chain.setFixedStages (1, 1);

            expect (chain.setOrder ({ 0, 2, 1, 3 }), "swapping the free middle was refused");

            expect (! chain.setOrder ({ 1, 0, 2, 3 }), "the pinned first stage was moved");
            expect (! chain.setOrder ({ 0, 1, 3, 2 }), "the pinned last stage was moved");
            expect (! chain.setOrder ({ 3, 1, 2, 0 }), "both pins were moved");

            juce::AudioBuffer<float> buffer (2, 64);
            buffer.clear();
            log.clear();
            chain.processBlock (buffer);

            expectEquals (juce::String (log), juce::String ("acbd"));
        }

        beginTest ("The order round-trips through getOrder");
        {
            std::string log;
            milodikfx::dsp::DSPChainManager chain;

            for (char c : { 'a', 'b', 'c', 'd', 'e' })
                chain.addProcessor (std::make_unique<MarkerProcessor> (c, log));

            const std::vector<int> wanted { 4, 1, 0, 3, 2 };
            expect (chain.setOrder (wanted));

            expect (chain.getOrder() == wanted, "getOrder did not report what was set");
        }

        beginTest ("A change lands on a block boundary, never inside one");
        {
            // The whole point of packing the permutation into one atomic: a block
            // runs entirely in the old order or entirely in the new one. A torn
            // read would show up here as a run that is neither -- a stage running
            // twice, or missing.
            std::string log;
            milodikfx::dsp::DSPChainManager chain;

            for (char c : { 'a', 'b', 'c', 'd', 'e', 'f' })
                chain.addProcessor (std::make_unique<MarkerProcessor> (c, log));

            chain.prepareToPlay (48000.0, 64, 2);

            std::atomic<bool> stop { false };
            std::atomic<int> accepted { 0 };

            std::thread reorderer ([&]
            {
                const std::vector<std::vector<int>> orders {
                    { 0, 1, 2, 3, 4, 5 },
                    { 5, 4, 3, 2, 1, 0 },
                    { 2, 0, 4, 1, 5, 3 },
                    { 1, 3, 5, 0, 2, 4 },
                };

                for (int i = 0; ! stop.load(); ++i)
                {
                    if (chain.setOrder (orders[(size_t) (i % orders.size())]))
                        accepted.fetch_add (1);
                }
            });

            juce::AudioBuffer<float> buffer (2, 64);
            buffer.clear();

            auto everyBlockWasWellFormed = true;

            for (int block = 0; block < 20000; ++block)
            {
                log.clear();
                chain.processBlock (buffer);

                // Six stages, each exactly once, whatever order they came in.
                if (log.size() != 6)
                {
                    everyBlockWasWellFormed = false;
                    break;
                }

                auto sorted = log;
                std::sort (sorted.begin(), sorted.end());

                if (sorted != "abcdef")
                {
                    everyBlockWasWellFormed = false;
                    break;
                }
            }

            stop.store (true);
            reorderer.join();

            logMessage ("  " + juce::String (accepted.load()) + " reorders during 20000 blocks");

            expect (accepted.load() > 0, "the reordering thread never ran");
            expect (everyBlockWasWellFormed,
                    "a block ran a torn order: a stage was doubled or dropped");
        }

        beginTest ("The real chain pins the input trim and the master stage");
        {
            milodikfx::dsp::DSPChainManager manager;
            const auto chain = milodikfx::dsp::buildGuitarChain (manager);

            const auto count = manager.getNumProcessors();
            expectEquals (count, 14);

            std::vector<int> order;
            for (int i = 0; i < count; ++i)
                order.push_back (i);

            // Swapping two stages in the middle -- overdrive and EQ, the swap
            // people actually want -- must be allowed.
            std::swap (order[4], order[5]);
            expect (manager.setOrder (order), "a middle swap was refused on the real chain");

            // Moving the trim off the front, or putting anything after the
            // master, must not be.
            std::vector<int> movesTrim;
            for (int i = 0; i < count; ++i)
                movesTrim.push_back (i);
            std::swap (movesTrim[0], movesTrim[1]);
            expect (! manager.setOrder (movesTrim), "the input trim was allowed off the front");

            std::vector<int> movesMaster;
            for (int i = 0; i < count; ++i)
                movesMaster.push_back (i);
            std::swap (movesMaster[count - 1], movesMaster[count - 2]);
            expect (! manager.setOrder (movesMaster),
                    "a stage was allowed after the master limiter");

            juce::ignoreUnused (chain);
        }
    }
};

static ChainOrderTests chainOrderTests;
} // namespace
