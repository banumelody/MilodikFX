#include <JuceHeader.h>

#include <memory>
#include <vector>

#include <algorithm>
#include <string>

#include "api/ParameterRegistry.h"
#include "dsp/CabinetProcessor.h"
#include "dsp/ChainOrder.h"
#include "dsp/ChainFactory.h"
#include "dsp/DSPChainManager.h"
#include "dsp/DelayProcessor.h"
#include "dsp/ReverbProcessor.h"

#if JUCE_WINDOWS
 #include <windows.h>
 #include <psapi.h>
#endif

namespace
{
constexpr double kRate = 96000.0;   // the worst case: buffers scale with it
constexpr int kBlock = 128;

/** Process working set in kilobytes, or 0 where it cannot be read. */
size_t workingSetKb()
{
#if JUCE_WINDOWS
    PROCESS_MEMORY_COUNTERS counters {};

    if (GetProcessMemoryInfo (GetCurrentProcess(), &counters, sizeof (counters)))
        return counters.WorkingSetSize / 1024;
#endif
    return 0;
}

/**
 * What a second instance of a block actually costs.
 *
 * v0.31 gives each block type a fixed inventory, and every instance is built at
 * startup whether it is on the board or not -- that is what keeps the parameter
 * registry immutable and the VST3 parameter list fixed. The price is paid in
 * memory up front, so the counts are chosen from measurements rather than from
 * taste. This logs them; the assertions are the ones that would still mean
 * something on a different machine.
 */
class InventoryCostTests final : public juce::UnitTest
{
public:
    InventoryCostTests() : juce::UnitTest ("Inventory cost", "milodikfx") {}

    void runTest() override
    {
        beginTest ("What one more of each block costs in memory");
        {
            // Measured at 96 kHz because every delay line and reverb tank is
            // sized for the highest rate it might be asked to run at.
            report<milodikfx::dsp::DelayProcessor> ("Delay");
            report<milodikfx::dsp::ReverbProcessor> ("Reverb");
            report<milodikfx::dsp::CabinetProcessor> ("Cabinet");

            expect (true);   // a measurement, not a threshold
        }

        beginTest ("A whole extra chain is affordable");
        {
            const auto before = workingSetKb();

            std::vector<std::unique_ptr<milodikfx::dsp::DSPChainManager>> chains;

            for (int i = 0; i < 4; ++i)
            {
                auto manager = std::make_unique<milodikfx::dsp::DSPChainManager>();
                milodikfx::dsp::buildGuitarChain (*manager, { false, false });
                manager->prepareToPlay (kRate, kBlock, 2);
                chains.push_back (std::move (manager));
            }

            const auto after = workingSetKb();
            const auto perChain = (after - before) / 4;

            logMessage ("  a full chain (no looper/metronome) costs ~"
                        + juce::String ((int) perChain) + " kB at 96 kHz");

            // The inventory adds roughly one extra chain's worth of stages, so
            // if a whole chain were tens of megabytes the plan would need
            // rethinking rather than measuring again later.
            expect (perChain < 64 * 1024,
                    "a chain costs more than 64 MB; the inventory plan needs revisiting");
        }
    }

private:
    template <typename T>
    void report (const char* name)
    {
        const auto before = workingSetKb();

        // Ten at a time: one instance is lost in the noise of the allocator.
        std::vector<std::unique_ptr<T>> instances;

        for (int i = 0; i < 10; ++i)
        {
            auto processor = std::make_unique<T>();
            processor->prepareToPlay (kRate, kBlock, 2);
            instances.push_back (std::move (processor));
        }

        const auto after = workingSetKb();

        logMessage ("  " + juce::String (name).paddedRight (' ', 10)
                    + juce::String ((int) ((after - before) / 10)) + " kB each"
                    + "   (sizeof " + juce::String ((int) sizeof (T)) + " B)");
    }
};

/**
 * Duplicate blocks, the Fractal way: a fixed inventory built at startup.
 *
 * The decision that makes this cheap is that **instance 1 keeps the bare id it
 * has always had**. Everything written before v0.31 -- presets, the settings
 * file, MIDI mappings, modifier targets, DAW automation lanes -- names those
 * ids, and they still mean exactly what they did.
 */
class InventoryTests final : public juce::UnitTest
{
public:
    InventoryTests() : juce::UnitTest ("Inventory", "milodikfx") {}

    void runTest() override
    {
        beginTest ("Instance 1 keeps the id it has always had");
        {
            milodikfx::dsp::DSPChainManager manager;
            const auto chain = milodikfx::dsp::buildGuitarChain (manager);
            milodikfx::dsp::ChainOrder order (chain, manager);

            const auto ids = order.getStageIds();
            const auto has = [&ids] (const std::string& id)
            {
                return std::find (ids.begin(), ids.end(), id) != ids.end();
            };

            // No suffix on the first of anything. A migration table would have
            // been needed for every layer of persistence otherwise.
            expect (has ("overdrive"), "instance 1 was renamed");
            expect (has ("delay"));
            expect (has ("cabinet"));
            expect (! has ("overdrive1"), "instance 1 must not gain a '1'");

            // ...and the extras are numbered from two.
            expect (has ("overdrive2"));
            expect (has ("overdrive3"));
            expect (has ("delay2"));
            expect (! has ("overdrive4"), "the inventory grew past its count");
            expect (! has ("nam2"), "the amp head must stay single");
            expect (! has ("split2"));
            expect (! has ("master2"));
        }

        beginTest ("Each instance has its own parameters");
        {
            milodikfx::dsp::DSPChainManager manager;
            const auto chain = milodikfx::dsp::buildGuitarChain (manager);

            milodikfx::api::ParameterRegistry registry;
            milodikfx::dsp::registerChainParameters (registry, chain, manager);

            const auto* first = registry.findParameter ("overdrive", "drivePct");
            const auto* second = registry.findParameter ("overdrive2", "drivePct");

            expect (first != nullptr && second != nullptr, "an instance is missing");

            first->set (10.0f);
            second->set (80.0f);

            // The whole point: two overdrives dialled differently. Sharing an
            // atomic here would make the second block a copy of the first.
            expectWithinAbsoluteError (first->get(), 10.0f, 0.5f);
            expectWithinAbsoluteError (second->get(), 80.0f, 0.5f);
            expectWithinAbsoluteError (chain.overdrive->getDrivePercent(), 10.0f, 0.5f);
            expectWithinAbsoluteError (chain.extras[0].overdrive->getDrivePercent(), 80.0f, 0.5f);
        }

        beginTest ("Every instance carries the same controls");
        {
            milodikfx::dsp::DSPChainManager manager;
            const auto chain = milodikfx::dsp::buildGuitarChain (manager);

            milodikfx::api::ParameterRegistry registry;
            milodikfx::dsp::registerChainParameters (registry, chain, manager);

            const auto* first = registry.findEffect ("overdrive");
            const auto* third = registry.findEffect ("overdrive3");

            expect (first != nullptr && third != nullptr);
            expectEquals ((int) third->parameters.size(), (int) first->parameters.size());

            // Described in one place and run per instance, so a later one cannot
            // quietly drift from the first as controls are added.
            for (size_t i = 0; i < first->parameters.size(); ++i)
                expect (first->parameters[i].id == third->parameters[i].id,
                        "instance 3 has a different control set");
        }

        beginTest ("Instances are labelled so two cards are told apart");
        {
            milodikfx::dsp::DSPChainManager manager;
            const auto chain = milodikfx::dsp::buildGuitarChain (manager);

            milodikfx::api::ParameterRegistry registry;
            milodikfx::dsp::registerChainParameters (registry, chain, manager);

            expect (registry.findEffect ("overdrive")->label == "Overdrive");
            expect (registry.findEffect ("overdrive2")->label == "Overdrive 2");
        }

        beginTest ("One tempo reaches every delay");
        {
            // Two independently-edited tempi would let a synced repeat drift
            // against the click. Before the inventory this was free; with more
            // than one delay it has to be done on purpose.
            milodikfx::dsp::DSPChainManager manager;
            const auto chain = milodikfx::dsp::buildGuitarChain (manager);

            milodikfx::api::ParameterRegistry registry;
            milodikfx::dsp::registerChainParameters (registry, chain, manager);

            const auto* bpm = registry.findParameter ("global", "bpm");
            expect (bpm != nullptr);

            bpm->set (152.0f);

            expectWithinAbsoluteError (chain.delay->getBpm(), 152.0f, 0.01f);
            expectWithinAbsoluteError (chain.extras[0].delay->getBpm(), 152.0f, 0.01f,
                                       "the second delay is running on a stale tempo");
        }

        beginTest ("The inventory is what decides how many exist");
        {
            milodikfx::dsp::DSPChainManager manager;

            milodikfx::dsp::ChainInventory lean;
            lean.overdrive = 1;
            lean.delay = 1;
            lean.reverb = 1;
            lean.cabinet = 1;
            lean.eq = 1;
            lean.toneStack = 1;
            lean.compressor = 1;
            lean.noiseGate = 1;
            lean.cleanBoost = 1;

            const auto chain = milodikfx::dsp::buildGuitarChain (manager, {}, lean);
            milodikfx::dsp::ChainOrder order (chain, manager);

            // One of everything is the pre-v0.31 chain exactly, which is what
            // makes the counts a tuning knob rather than an architecture.
            expectEquals ((int) order.getStageIds().size(), 14);
            expect (chain.extras.empty());
        }
    }
};

static InventoryTests inventoryTests;
static InventoryCostTests inventoryCostTests;
} // namespace
