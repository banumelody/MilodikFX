#include <JuceHeader.h>

#include <algorithm>

#include "dsp/ChainFactory.h"
#include "dsp/DSPChainManager.h"
#include "dsp/ModulationEngine.h"

//==============================================================================
class ModulationEngineTests final : public juce::UnitTest
{
public:
    ModulationEngineTests() : juce::UnitTest ("ModulationEngine", "dsp") {}

    void runTest() override
    {
        using milodikfx::dsp::ModulationEngine;
        using Source = ModulationEngine::Source;

        milodikfx::dsp::DSPChainManager manager;
        const auto chain = milodikfx::dsp::buildGuitarChain (manager);

        milodikfx::api::ParameterRegistry registry;
        milodikfx::dsp::registerChainParameters (registry, chain, manager);

        const auto* drive = registry.findParameter ("overdrive", "drivePct");
        expect (drive != nullptr);
        if (drive == nullptr)
            return;

        beginTest ("An LFO sweeps the target between low and high");
        {
            ModulationEngine mod;
            mod.prepare (48000.0);

            expect (mod.setModifier (0, drive, "overdrive", "drivePct", Source::lfoSine, 0.0f, 100.0f, 5.0f));

            float lo = 1.0e9f;
            float hi = -1.0e9f;

            // 20 blocks of 512 at 48 kHz and 5 Hz is a bit over one full cycle.
            for (int b = 0; b < 20; ++b)
            {
                mod.process (0.0f, 512);
                const auto v = drive->get();
                lo = std::min (lo, v);
                hi = std::max (hi, v);
            }

            expect (lo < 10.0f, "LFO low end not reached: " + juce::String (lo));
            expect (hi > 90.0f, "LFO high end not reached: " + juce::String (hi));
        }

        beginTest ("Removing a modifier restores the value it had before");
        {
            ModulationEngine mod;
            mod.prepare (48000.0);

            float applied = 0.0f;
            registry.setParameter ("overdrive", "drivePct", 42.0f, applied);

            mod.setModifier (0, drive, "overdrive", "drivePct", Source::lfoSine, 0.0f, 100.0f, 5.0f);
            mod.process (0.0f, 512);
            expect (std::abs (drive->get() - 42.0f) > 1.0f, "the modifier did not move the value");

            mod.clearModifier (0);
            mod.process (0.0f, 512); // the audio thread restores on the first inactive block

            expectWithinAbsoluteError (drive->get(), 42.0f, 0.5f);
        }

        beginTest ("The envelope follower opens the sweep with the input level");
        {
            ModulationEngine mod;
            mod.prepare (48000.0);

            mod.setModifier (0, drive, "overdrive", "drivePct", Source::envelope, 0.0f, 100.0f, 1.0f);

            for (int b = 0; b < 40; ++b)
                mod.process (0.0f, 512); // silence -> settle low
            const auto quiet = drive->get();

            for (int b = 0; b < 40; ++b)
                mod.process (0.5f, 512); // a hard pick -> open up
            const auto loud = drive->get();

            expect (loud > quiet + 20.0f,
                    "envelope did not follow the input: quiet " + juce::String (quiet)
                        + " loud " + juce::String (loud));
        }

        beginTest ("The swept value stays inside the target's range");
        {
            ModulationEngine mod;
            mod.prepare (48000.0);

            // Deliberately wider than 0..100 so the clamp has to bite.
            mod.setModifier (0, drive, "overdrive", "drivePct", Source::lfoSine, -50.0f, 200.0f, 5.0f);

            for (int b = 0; b < 20; ++b)
            {
                mod.process (0.0f, 512);
                const auto v = drive->get();
                expect (v >= 0.0f && v <= 100.0f, "value left the range: " + juce::String (v));
            }
        }

        beginTest ("A bad slot or an unsuitable target is refused");
        {
            ModulationEngine mod;
            mod.prepare (48000.0);

            const auto* pingPong = registry.findParameter ("delay", "pingPong"); // a switch

            expect (! mod.setModifier (-1, drive, "overdrive", "drivePct", Source::lfoSine, 0, 100, 5));
            expect (! mod.setModifier (4, drive, "overdrive", "drivePct", Source::lfoSine, 0, 100, 5));
            expect (! mod.setModifier (0, nullptr, "", "", Source::lfoSine, 0, 100, 5));

            if (pingPong != nullptr)
                expect (! mod.setModifier (0, pingPong, "delay", "pingPong", Source::lfoSine, 0, 1, 1),
                        "a boolean switch should not be modulatable");

            expect (! ModulationEngine::isValidSlot (-1));
            expect (! ModulationEngine::isValidSlot (ModulationEngine::kMaxModifiers));
        }

        beginTest ("A modifier reports its binding for the API");
        {
            ModulationEngine mod;
            mod.prepare (48000.0);

            mod.setModifier (1, drive, "overdrive", "drivePct", Source::lfoTriangle, 10.0f, 90.0f, 3.0f);

            const auto info = mod.getModifier (1);
            expect (info.active);
            expectEquals (juce::String (info.effectId), juce::String ("overdrive"));
            expectEquals (juce::String (info.parameterId), juce::String ("drivePct"));
            expectEquals (info.source, (int) Source::lfoTriangle);
            expectWithinAbsoluteError (info.low, 10.0f, 0.001f);
            expectWithinAbsoluteError (info.high, 90.0f, 0.001f);

            mod.clearModifier (1);
            expect (! mod.getModifier (1).active);
        }

        beginTest ("Reports the base value for persistence while sweeping");
        {
            ModulationEngine mod;
            mod.prepare (48000.0);

            float applied = 0.0f;
            registry.setParameter ("overdrive", "drivePct", 33.0f, applied);

            mod.setModifier (0, drive, "overdrive", "drivePct", Source::lfoSine, 0.0f, 100.0f, 5.0f);
            mod.process (0.0f, 512); // the live value has moved off 33

            float base = 0.0f;
            expect (mod.getBaseValue ("overdrive", "drivePct", base), "no base reported for a modulated param");
            expectWithinAbsoluteError (base, 33.0f, 0.5f);

            // A parameter no modifier owns has no override.
            float other = 0.0f;
            expect (! mod.getBaseValue ("delay", "mixPct", other));

            mod.clearModifier (0);
            expect (! mod.getBaseValue ("overdrive", "drivePct", base),
                    "a cleared modifier should stop reporting a base");
        }
    }
};

static ModulationEngineTests modulationEngineTests;
