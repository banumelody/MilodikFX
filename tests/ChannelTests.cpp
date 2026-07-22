#include <JuceHeader.h>

#include "dsp/ChainFactory.h"
#include "dsp/DSPChainManager.h"
#include "preset/ChannelStore.h"

//==============================================================================
class ChannelStoreTests final : public juce::UnitTest
{
public:
    ChannelStoreTests() : juce::UnitTest ("ChannelStore", "preset") {}

    void runTest() override
    {
        using milodikfx::preset::ChannelStore;

        milodikfx::dsp::DSPChainManager manager;
        const auto chain = milodikfx::dsp::buildGuitarChain (manager);

        milodikfx::api::ParameterRegistry registry;
        milodikfx::dsp::registerChainParameters (registry, chain, manager);

        auto drive = [&registry] { return registry.findParameter ("overdrive", "drivePct")->get(); };
        auto setDrive = [&registry] (float v) { float a; registry.setParameter ("overdrive", "drivePct", v, a); };

        beginTest ("An effect starts on channel A named A-D");
        {
            ChannelStore channels (registry);

            expectEquals (channels.getActive ("overdrive"), 0);
            expectEquals (channels.getName ("overdrive", 0), juce::String ("A"));
            expectEquals (channels.getName ("overdrive", 1), juce::String ("B"));
            expectEquals (channels.getName ("overdrive", 2), juce::String ("C"));
            expectEquals (channels.getName ("overdrive", 3), juce::String ("D"));
        }

        beginTest ("Switching channels stores and restores a per-channel sound");
        {
            // The heart of it: one drive block holding two different gains, jumped
            // between without a second block. Switching saves the outgoing
            // channel first, so edits made on it are kept.
            ChannelStore channels (registry);

            setDrive (30.0f);
            expect (channels.recall ("overdrive", 1)); // save A=30, load B (also 30 so far)
            expectEquals (channels.getActive ("overdrive"), 1);

            setDrive (90.0f);                           // dial B's sound
            expect (channels.recall ("overdrive", 0));  // save B=90, load A
            expectWithinAbsoluteError (drive(), 30.0f, 0.02f);

            expect (channels.recall ("overdrive", 1));  // back to B
            expectWithinAbsoluteError (drive(), 90.0f, 0.02f);
        }

        beginTest ("An explicit save overwrites a channel with the live sound");
        {
            ChannelStore channels (registry);

            setDrive (12.0f);
            expect (channels.save ("overdrive", 2)); // C = 12, active unchanged (still 0)

            setDrive (55.0f);
            expect (channels.recall ("overdrive", 2)); // save A=live, load C=12
            expectWithinAbsoluteError (drive(), 12.0f, 0.02f);
        }

        beginTest ("Selecting a channel touches only its own effect");
        {
            ChannelStore channels (registry);

            float applied = 0.0f;
            registry.setParameter ("delay", "mixPct", 40.0f, applied);
            const auto delayBefore = registry.findParameter ("delay", "mixPct")->get();

            setDrive (20.0f);
            channels.recall ("overdrive", 1);
            channels.recall ("overdrive", 0);

            expectWithinAbsoluteError (registry.findParameter ("delay", "mixPct")->get(), delayBefore, 0.001f);
        }

        beginTest ("toVar carries each channel's stored values");
        {
            ChannelStore channels (registry);

            setDrive (25.0f);
            channels.recall ("overdrive", 1); // A=25, load B(25), active 1
            setDrive (75.0f);
            channels.recall ("overdrive", 0); // B=75, load A(25), active 0
            channels.setName ("overdrive", 1, "Lead");

            const auto v = channels.toVar();
            const auto slots = v["overdrive"]["slots"];

            expect (slots.isArray());
            expectWithinAbsoluteError ((float) (double) slots[0]["params"]["drivePct"], 25.0f, 0.02f);
            expectWithinAbsoluteError ((float) (double) slots[1]["params"]["drivePct"], 75.0f, 0.02f);
            expectEquals (slots[1]["name"].toString(), juce::String ("Lead"));
        }

        beginTest ("Channels survive a round trip through JSON");
        {
            juce::var parsed;
            const auto ok = juce::JSON::parse (R"({"overdrive":{"active":1,"slots":[
                {"name":"Rhythm","populated":true,"params":{"drivePct":25.0},"text":{}},
                {"name":"Lead","populated":true,"params":{"drivePct":75.0},"text":{}},
                {"name":"C","populated":true,"params":{"drivePct":50.0},"text":{}},
                {"name":"D","populated":true,"params":{"drivePct":50.0},"text":{}}
            ]}})", parsed);
            expect (ok.wasOk());

            ChannelStore restored (registry);
            restored.fromVar (parsed);

            expectEquals (restored.getActive ("overdrive"), 1);
            expectEquals (restored.getName ("overdrive", 0), juce::String ("Rhythm"));
            expectEquals (restored.getName ("overdrive", 1), juce::String ("Lead"));

            // Load A and confirm its stored value came back. (Switching away from
            // the active channel saves the live sound into it, so we verify A by
            // loading it, not by leaving it.)
            expect (restored.recall ("overdrive", 0));
            expectWithinAbsoluteError (drive(), 25.0f, 0.02f);
        }

        beginTest ("A bad index or unknown effect is refused, not half-applied");
        {
            ChannelStore channels (registry);

            expect (! channels.recall ("overdrive", -1));
            expect (! channels.recall ("overdrive", ChannelStore::kNumChannels));
            expect (! channels.recall ("nosuch", 0));
            expect (! channels.save ("nosuch", 0));
            expect (! channels.setName ("overdrive", 9, "X"));
            expect (! channels.setName ("overdrive", 0, "   "));
            expect (! ChannelStore::isValidIndex (-1));
            expect (! ChannelStore::isValidIndex (ChannelStore::kNumChannels));
        }

        beginTest ("Rubbish in the stored channels is ignored, not half-applied");
        {
            ChannelStore channels (registry);

            channels.fromVar (juce::var ("not an object"));
            // Nothing stored; an effect still reports channel A by default.
            expectEquals (channels.getActive ("overdrive"), 0);
            expectEquals (channels.getName ("overdrive", 0), juce::String ("A"));
        }

        beginTest ("resetToCurrent seeds every effect from the chain as it stands");
        {
            ChannelStore channels (registry);

            setDrive (63.0f);
            channels.resetToCurrent();

            const auto v = channels.toVar();
            expect (v["overdrive"].isObject(), "overdrive got no channels");
            expectWithinAbsoluteError ((float) (double) v["overdrive"]["slots"][0]["params"]["drivePct"], 63.0f, 0.02f);
        }
    }
};

static ChannelStoreTests channelStoreTests;
