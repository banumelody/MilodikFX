#include <JuceHeader.h>

#include "dsp/ChainFactory.h"
#include "preset/PinnedControls.h"

namespace
{
class PinnedControlsTests final : public juce::UnitTest
{
public:
    PinnedControlsTests() : juce::UnitTest ("PinnedControls") {}

    void runTest() override
    {
        using milodikfx::preset::PinnedControls;

        beginTest ("Adding, toggling and removing");

        PinnedControls pins;
        expectEquals (pins.getNumPins(), 0);

        expect (pins.add ("overdrive", "drivePct"));
        expect (pins.isPinned ("overdrive", "drivePct"));
        expectEquals (pins.getNumPins(), 1);

        // Pinning the same control twice would give the stage screen two knobs
        // driving one parameter.
        expect (! pins.add ("overdrive", "drivePct"), "a duplicate pin must be refused");
        expectEquals (pins.getNumPins(), 1);

        expect (! pins.toggle ("overdrive", "drivePct"), "toggle must remove an existing pin");
        expectEquals (pins.getNumPins(), 0);

        expect (pins.toggle ("delay", "mixPct"), "toggle must add a missing pin");
        expectEquals (pins.getNumPins(), 1);

        expect (! pins.remove ("reverb", "decayTime"), "removing an absent pin must report false");
        expect (pins.remove ("delay", "mixPct"));
        expectEquals (pins.getNumPins(), 0);

        beginTest ("Removal closes the gap, keeping the order stable");

        // The order is the order they appear on stage. Leaving a hole -- or
        // swapping the last one into the gap -- would shuffle the remaining
        // knobs under the player's fingers.
        pins.clear();
        pins.add ("a", "1");
        pins.add ("b", "2");
        pins.add ("c", "3");
        pins.add ("d", "4");

        expect (pins.remove ("b", "2"));
        expectEquals (pins.getNumPins(), 3);
        expect (pins.getPin (0).effectId == "a");
        expect (pins.getPin (1).effectId == "c", "the survivors must keep their order");
        expect (pins.getPin (2).effectId == "d");

        beginTest ("The cap is enforced");

        pins.clear();

        for (int i = 0; i < PinnedControls::kMaxPins; ++i)
            expect (pins.add ("effect" + std::to_string (i), "param"), "pin " + juce::String (i) + " refused");

        expectEquals (pins.getNumPins(), PinnedControls::kMaxPins);

        // Unbounded pins would quietly turn the Perform view back into the rack
        // it exists to avoid.
        expect (! pins.add ("oneTooMany", "param"), "the cap must be enforced");
        expectEquals (pins.getNumPins(), PinnedControls::kMaxPins);

        beginTest ("Empty ids are refused");

        pins.clear();
        expect (! pins.add ("", "drivePct"));
        expect (! pins.add ("overdrive", ""));
        expectEquals (pins.getNumPins(), 0);

        beginTest ("JSON round trip");

        pins.clear();
        pins.add ("overdrive", "drivePct");
        pins.add ("delay", "mixPct");

        const auto stored = juce::JSON::toString (pins.toVar(), true);

        juce::var parsed;
        expect (juce::JSON::parse (stored, parsed).wasOk());

        PinnedControls restored;
        restored.fromVar (parsed);

        expectEquals (restored.getNumPins(), 2);
        expect (restored.isPinned ("overdrive", "drivePct"));
        expect (restored.isPinned ("delay", "mixPct"));
        expect (restored.getPin (0).effectId == "overdrive", "order must survive the round trip");

        beginTest ("Pins are validated against the registry");

        milodikfx::dsp::DSPChainManager manager;
        const auto chain = milodikfx::dsp::buildGuitarChain (manager);

        milodikfx::api::ParameterRegistry registry;
        milodikfx::dsp::registerChainParameters (registry, chain, manager);

        juce::Array<juce::var> array;

        const auto entry = [] (const char* effect, const char* parameter)
        {
            auto* object = new juce::DynamicObject();
            object->setProperty ("effect", effect);
            object->setProperty ("parameter", parameter);
            return juce::var (object);
        };

        array.add (entry ("overdrive", "drivePct"));
        array.add (entry ("phaser", "rateHz"));       // no such effect in this build
        array.add (entry ("delay", "nonexistent"));   // effect exists, parameter does not
        array.add (entry ("reverb", "decayTime"));

        PinnedControls checked;
        checked.fromVar (juce::var (array), &registry);

        // A pin naming something this build does not have would put a knob on the
        // stage screen with nothing behind it.
        expectEquals (checked.getNumPins(), 2);
        expect (checked.isPinned ("overdrive", "drivePct"));
        expect (checked.isPinned ("reverb", "decayTime"));
        expect (! checked.isPinned ("phaser", "rateHz"), "an unknown effect must be dropped");
        expect (! checked.isPinned ("delay", "nonexistent"), "an unknown parameter must be dropped");

        beginTest ("Junk deserialises to nothing rather than throwing");

        PinnedControls junk;
        junk.add ("overdrive", "drivePct");

        junk.fromVar (juce::var ("not an array"));
        expectEquals (junk.getNumPins(), 0, "a non-array must clear the list");

        junk.fromVar (juce::var());
        expectEquals (junk.getNumPins(), 0);

        beginTest ("A stored list longer than the cap is truncated, not refused");

        juce::Array<juce::var> tooMany;

        for (int i = 0; i < PinnedControls::kMaxPins + 4; ++i)
            tooMany.add (entry (("effect" + juce::String (i)).toRawUTF8(), "param"));

        PinnedControls capped;
        capped.fromVar (juce::var (tooMany));

        expectEquals (capped.getNumPins(), PinnedControls::kMaxPins);
    }
};

static PinnedControlsTests pinnedControlsTests;
} // namespace
