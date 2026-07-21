#include <JuceHeader.h>

#include "dsp/ChainFactory.h"
#include "dsp/DSPChainManager.h"
#include "midi/MidiController.h"

namespace
{
using milodikfx::midi::Mapping;
using milodikfx::midi::MappingMode;

Mapping makeMapping (const char* effect, const char* parameter, MappingMode mode = MappingMode::continuous)
{
    Mapping mapping;
    mapping.effectId = effect;
    mapping.parameterId = parameter;
    mapping.mode = mode;
    return mapping;
}

/** A control change on channel 1, as a footswitch or knob would send it. */
juce::MidiMessage cc (int controller, int value)
{
    return juce::MidiMessage::controllerEvent (1, controller, value);
}
} // namespace

//==============================================================================
class MidiMappingTests final : public juce::UnitTest
{
public:
    MidiMappingTests() : juce::UnitTest ("MIDI mapping", "midi") {}

    void runTest() override
    {
        using milodikfx::midi::MidiController;

        milodikfx::dsp::DSPChainManager manager;
        const auto chain = milodikfx::dsp::buildGuitarChain (manager);

        milodikfx::api::ParameterRegistry registry;
        milodikfx::dsp::registerChainParameters (registry, chain, manager);

        MidiController controller (registry);

        beginTest ("Nothing is mapped to begin with");
        {
            expect (controller.getMappings().empty());
            expect (! controller.getMapping (7).isValid());
            expect (! controller.isLearning());
            expectEquals (controller.getLastControllerNumber(), -1);
        }

        beginTest ("A mapping can be set, read back and cleared");
        {
            controller.setMapping (7, makeMapping ("overdrive", "drivePct"));

            const auto stored = controller.getMapping (7);
            expectEquals (stored.effectId, juce::String ("overdrive"));
            expectEquals (stored.parameterId, juce::String ("drivePct"));
            expect (stored.mode == MappingMode::continuous);

            expectEquals ((int) controller.getMappings().size(), 1);

            controller.clearMapping (7);
            expect (! controller.getMapping (7).isValid());
            expect (controller.getMappings().empty());
        }

        beginTest ("Controller numbers outside 0-127 are refused, not wrapped");
        {
            // A wrapped index would silently rebind an unrelated controller.
            controller.setMapping (-1, makeMapping ("overdrive", "drivePct"));
            controller.setMapping (128, makeMapping ("overdrive", "drivePct"));
            controller.setMapping (99999, makeMapping ("overdrive", "drivePct"));

            expect (controller.getMappings().empty());
            expect (! controller.getMapping (-1).isValid());
            expect (! controller.getMapping (128).isValid());
        }

        beginTest ("Mappings come back in controller order");
        {
            controller.setMapping (64, makeMapping ("reverb", "dryWetMix"));
            controller.setMapping (7, makeMapping ("master", "volumeDb"));
            controller.setMapping (11, makeMapping ("overdrive", "drivePct"));

            const auto all = controller.getMappings();

            expectEquals ((int) all.size(), 3);
            expectEquals (all[0].first, 7);
            expectEquals (all[1].first, 11);
            expectEquals (all[2].first, 64);

            for (int cc = 0; cc < MidiController::kNumControllers; ++cc)
                controller.clearMapping (cc);
        }

        beginTest ("Learn mode arms and disarms");
        {
            expect (! controller.isLearning());

            controller.startLearning (makeMapping ("delay", "mixPct"));
            expect (controller.isLearning());
            expectEquals (controller.getLearnTarget().parameterId, juce::String ("mixPct"));

            controller.stopLearning();
            expect (! controller.isLearning());

            // An incomplete target is not something to wait on -- it could never
            // resolve to a parameter.
            controller.startLearning (makeMapping ("delay", ""));
            expect (! controller.isLearning());
        }
    }
};

static MidiMappingTests midiMappingTests;

//==============================================================================
/**
 * What actually happens when a message arrives.
 *
 * Messages are injected through handleIncomingMidiMessage rather than a real
 * port: a build machine has no controller, and a virtual MIDI driver cannot be
 * assumed either.
 */
class MidiDispatchTests final : public juce::UnitTest
{
public:
    MidiDispatchTests() : juce::UnitTest ("MIDI dispatch", "midi") {}

    void runTest() override
    {
        using milodikfx::midi::MidiController;

        milodikfx::dsp::DSPChainManager manager;
        const auto chain = milodikfx::dsp::buildGuitarChain (manager);

        milodikfx::api::ParameterRegistry registry;
        milodikfx::dsp::registerChainParameters (registry, chain, manager);

        MidiController controller (registry);

        const auto* drive = registry.findParameter ("overdrive", "drivePct");
        const auto* overdrive = registry.findEffect ("overdrive");

        expect (drive != nullptr && overdrive != nullptr);

        if (drive == nullptr || overdrive == nullptr)
            return;

        beginTest ("A knob sweep drives the parameter across its whole range");
        {
            controller.setMapping (11, makeMapping ("overdrive", "drivePct"));

            controller.handleIncomingMidiMessage (nullptr, cc (11, 0));
            expectWithinAbsoluteError (drive->get(), drive->minValue, 0.01f);

            controller.handleIncomingMidiMessage (nullptr, cc (11, 127));
            expectWithinAbsoluteError (drive->get(), drive->maxValue, 0.01f);

            controller.handleIncomingMidiMessage (nullptr, cc (11, 64));
            const auto expected = drive->minValue
                                  + (64.0f / 127.0f) * (drive->maxValue - drive->minValue);
            expectWithinAbsoluteError (drive->get(), expected, 0.5f);
        }

        beginTest ("An unmapped controller is ignored");
        {
            const auto before = drive->get();
            controller.handleIncomingMidiMessage (nullptr, cc (99, 0));
            expectWithinAbsoluteError (drive->get(), before, 0.001f);
        }

        beginTest ("A footswitch toggles on press and does nothing on release");
        {
            // The bug this guards: with a continuous mapping a footswitch sends
            // 127 on press and 0 on release, so the effect would only stay on
            // while the switch was held down.
            controller.setMapping (64, makeMapping ("overdrive", "enabled", MappingMode::toggle));

            registry.setEffectEnabled ("overdrive", false);
            expect (! overdrive->isEnabled());

            controller.handleIncomingMidiMessage (nullptr, cc (64, 127)); // press
            expect (overdrive->isEnabled(), "the press did not switch it on");

            controller.handleIncomingMidiMessage (nullptr, cc (64, 0)); // release
            expect (overdrive->isEnabled(), "the release switched it back off");

            controller.handleIncomingMidiMessage (nullptr, cc (64, 127)); // press again
            expect (! overdrive->isEnabled(), "the second press did not switch it off");

            controller.handleIncomingMidiMessage (nullptr, cc (64, 0));
            expect (! overdrive->isEnabled());
        }

        beginTest ("A continuous mapping onto a switch follows the pedal instead");
        {
            controller.setMapping (65, makeMapping ("overdrive", "enabled", MappingMode::continuous));

            controller.handleIncomingMidiMessage (nullptr, cc (65, 127));
            expect (overdrive->isEnabled());

            controller.handleIncomingMidiMessage (nullptr, cc (65, 0));
            expect (! overdrive->isEnabled());
        }

        beginTest ("A stage that is always in the path cannot be switched off by a controller");
        {
            controller.setMapping (66, makeMapping ("master", "enabled", MappingMode::toggle));

            const auto* master = registry.findEffect ("master");
            expect (master != nullptr);

            if (master != nullptr)
            {
                controller.handleIncomingMidiMessage (nullptr, cc (66, 127));
                expect (master->isEnabled(), "a footswitch silenced the whole app");
            }
        }

        beginTest ("Learn binds the first control that moves, and does not also act on it");
        {
            for (int n = 0; n < MidiController::kNumControllers; ++n)
                controller.clearMapping (n);

            registry.setEffectEnabled ("overdrive", false);

            auto learnedCc = -1;
            controller.onLearned = [&learnedCc] (int number, Mapping) { learnedCc = number; };

            controller.startLearning (makeMapping ("overdrive", "enabled", MappingMode::toggle));

            controller.handleIncomingMidiMessage (nullptr, cc (33, 127));

            expectEquals (learnedCc, 33);
            expect (controller.getMapping (33).isValid());
            expect (! controller.isLearning(), "learn stayed armed after binding");

            // Stepping on a switch to assign it must not also toggle the thing
            // it was just assigned to.
            expect (! overdrive->isEnabled(), "the binding press also fired the mapping");

            // ...but the next press does.
            controller.handleIncomingMidiMessage (nullptr, cc (33, 0));
            controller.handleIncomingMidiMessage (nullptr, cc (33, 127));
            expect (overdrive->isEnabled());

            controller.onLearned = nullptr;
        }

        beginTest ("The last controller seen is reported even when nothing is mapped");
        {
            // This is what separates "nothing is arriving" from "it arrives but
            // is not bound" when a rig will not respond.
            controller.handleIncomingMidiMessage (nullptr, cc (77, 12));

            expectEquals (controller.getLastControllerNumber(), 77);
            expectEquals (controller.getLastControllerValue(), 12);
        }

        beginTest ("A program change asks for a preset by number");
        {
            auto seen = -1;
            controller.onProgramChange = [&seen] (int program) { seen = program; };

            controller.handleIncomingMidiMessage (nullptr, juce::MidiMessage::programChange (1, 5));
            expectEquals (seen, 5);

            controller.onProgramChange = nullptr;
        }

        beginTest ("Notes and other traffic are ignored");
        {
            const auto before = drive->get();

            controller.handleIncomingMidiMessage (nullptr, juce::MidiMessage::noteOn (1, 60, 1.0f));
            controller.handleIncomingMidiMessage (nullptr, juce::MidiMessage::noteOff (1, 60));
            controller.handleIncomingMidiMessage (nullptr, juce::MidiMessage::pitchWheel (1, 8000));

            expectWithinAbsoluteError (drive->get(), before, 0.001f);
        }

        beginTest ("A mapping pointing at something that no longer exists is survivable");
        {
            controller.setMapping (20, makeMapping ("ghost", "mystery"));
            controller.setMapping (21, makeMapping ("overdrive", "mystery"));

            controller.handleIncomingMidiMessage (nullptr, cc (20, 100));
            controller.handleIncomingMidiMessage (nullptr, cc (21, 100));

            expect (true, "dispatching a dead mapping did not crash");
        }

        beginTest ("Every mapped write is announced, so it reaches the settings file");
        {
            // Without this a controller move was lost on the next launch: the
            // registry's own change hook is not on this path.
            auto announced = 0;
            controller.onParameterChanged = [&announced] { ++announced; };

            controller.setMapping (11, makeMapping ("overdrive", "drivePct"));
            controller.handleIncomingMidiMessage (nullptr, cc (11, 90));

            expectEquals (announced, 1);

            controller.onParameterChanged = nullptr;
        }
    }
};

static MidiDispatchTests midiDispatchTests;

//==============================================================================
/**
 * What a control change actually does to the chain.
 *
 * MidiController::handleIncomingMidiMessage is private and driven by JUCE, so
 * these exercise the same logic through the public surface: set a mapping, then
 * check the registry contract the callback relies on. The message dispatch
 * itself needs a physical controller and is verified by hand.
 */
class MidiValueScalingTests final : public juce::UnitTest
{
public:
    MidiValueScalingTests() : juce::UnitTest ("MIDI value scaling", "midi") {}

    void runTest() override
    {
        milodikfx::dsp::DSPChainManager manager;
        const auto chain = milodikfx::dsp::buildGuitarChain (manager);

        milodikfx::api::ParameterRegistry registry;
        milodikfx::dsp::registerChainParameters (registry, chain, manager);

        beginTest ("A controller's full sweep covers the parameter's full range");
        {
            const auto* parameter = registry.findParameter ("overdrive", "drivePct");
            expect (parameter != nullptr);

            if (parameter == nullptr)
                return;

            const struct { int cc; float expected; } cases[] = {
                { 0, parameter->minValue },
                { 127, parameter->maxValue },
                { 64, parameter->minValue + (64.0f / 127.0f) * (parameter->maxValue - parameter->minValue) },
            };

            for (const auto& c : cases)
            {
                const auto position = (float) c.cc / 127.0f;
                const auto scaled = parameter->minValue
                                    + position * (parameter->maxValue - parameter->minValue);

                float applied = 0.0f;
                expect (registry.setParameter ("overdrive", "drivePct", scaled, applied));
                expectWithinAbsoluteError (applied, c.expected, 0.01f);
            }
        }

        beginTest ("A parameter that does not go from zero still spans its own range");
        {
            // Delay time runs 10..1000 ms, so CC 0 must land on 10 rather than 0
            // -- a controller sweep that stopped short of either end would make
            // the pedal feel broken.
            const auto* parameter = registry.findParameter ("delay", "timeMs");
            expect (parameter != nullptr);

            if (parameter == nullptr)
                return;

            float applied = 0.0f;

            registry.setParameter ("delay", "timeMs", parameter->minValue, applied);
            expectWithinAbsoluteError (applied, parameter->minValue, 0.01f);

            registry.setParameter ("delay", "timeMs", parameter->maxValue, applied);
            expectWithinAbsoluteError (applied, parameter->maxValue, 0.01f);
        }

        beginTest ("A value beyond the range is clamped rather than refused");
        {
            float applied = 0.0f;

            expect (registry.setParameter ("master", "volumeDb", 9999.0f, applied));
            expectWithinAbsoluteError (applied,
                                       milodikfx::dsp::MasterOutProcessor::kMaxVolumeDb, 0.01f);

            expect (registry.setParameter ("master", "volumeDb", -9999.0f, applied));
            expectWithinAbsoluteError (applied,
                                       milodikfx::dsp::MasterOutProcessor::kMinVolumeDb, 0.01f);
        }

        beginTest ("A footswitch flips a boolean rather than tracking its value");
        {
            // The behaviour a toggle mapping produces: read, invert, write. A
            // footswitch sends 127 on press and 0 on release, so a continuous
            // mapping would need the switch held down to keep the effect on.
            const auto* parameter = registry.findParameter ("delay", "pingPong");
            expect (parameter != nullptr);

            if (parameter == nullptr)
                return;

            const auto midpoint = (parameter->minValue + parameter->maxValue) * 0.5f;

            for (int press = 0; press < 4; ++press)
            {
                const auto before = parameter->get();
                const auto next = before >= midpoint ? parameter->minValue : parameter->maxValue;

                float applied = 0.0f;
                registry.setParameter ("delay", "pingPong", next, applied);

                expect (parameter->get() != before, "a press did not change the value");
            }
        }

        beginTest ("An effect switch is reachable by name, since that is what goes on a footswitch");
        {
            const auto* effect = registry.findEffect ("overdrive");
            expect (effect != nullptr && effect->setEnabled != nullptr);

            if (effect == nullptr || effect->setEnabled == nullptr)
                return;

            const auto before = effect->isEnabled();
            registry.setEffectEnabled ("overdrive", ! before);
            expect (effect->isEnabled() != before);

            registry.setEffectEnabled ("overdrive", before);
        }

        beginTest ("A stage that is always in the path refuses to be switched off");
        {
            // Putting master under a footswitch by accident would silence the
            // whole app with nothing on screen to explain it.
            const auto* effect = registry.findEffect ("master");
            expect (effect != nullptr);

            if (effect == nullptr)
                return;

            expect (effect->setEnabled == nullptr);
            expect (! registry.setEffectEnabled ("master", false));
            expect (effect->isEnabled());
        }

        beginTest ("A mapping pointing at nothing is ignored rather than crashing");
        {
            float applied = 0.0f;

            expect (! registry.setParameter ("nosuch", "drivePct", 1.0f, applied));
            expect (! registry.setParameter ("overdrive", "nosuch", 1.0f, applied));
            expect (registry.findParameter ("nosuch", "nosuch") == nullptr);
            expect (registry.findEffect ("nosuch") == nullptr);
        }

        beginTest ("A text parameter is not something a knob can sweep");
        {
            // Nothing sensible maps CC 0..127 onto a list of filenames, so the
            // controller has to skip these rather than write a number into one.
            milodikfx::preset::IrLibrary library (
                juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getChildFile ("MilodikFX_MidiTests_Ir"));

            milodikfx::dsp::DSPChainManager irManager;
            const auto irChain = milodikfx::dsp::buildGuitarChain (irManager);

            milodikfx::api::ParameterRegistry irRegistry;
            milodikfx::dsp::ChainExtras extras;
            extras.irLibrary = &library;
            milodikfx::dsp::registerChainParameters (irRegistry, irChain, irManager, std::move (extras));

            const auto* parameter = irRegistry.findParameter ("cabinet", "irFile");
            expect (parameter != nullptr);

            if (parameter != nullptr)
            {
                expect (parameter->isText);
                expect (parameter->set == nullptr);
            }

            juce::File::getSpecialLocation (juce::File::tempDirectory)
                .getChildFile ("MilodikFX_MidiTests_Ir")
                .deleteRecursively();
        }
    }
};

static MidiValueScalingTests midiValueScalingTests;
