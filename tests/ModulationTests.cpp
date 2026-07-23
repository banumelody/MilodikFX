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

    using Engine = milodikfx::dsp::ModulationEngine;
    using Source = Engine::Source;
    using Config = Engine::Config;

    static Config lfo (float low, float high, float rateHz, Source source = Source::lfoSine)
    {
        Config c;
        c.source = source;
        c.low = low;
        c.high = high;
        c.rateHz = rateHz;
        return c;
    }

    void runTest() override
    {
        milodikfx::dsp::DSPChainManager manager;
        const auto chain = milodikfx::dsp::buildGuitarChain (manager);

        milodikfx::api::ParameterRegistry registry;
        milodikfx::dsp::registerChainParameters (registry, chain, manager);

        const auto* drive = registry.findParameter ("overdrive", "drivePct");
        expect (drive != nullptr);
        if (drive == nullptr)
            return;

        beginTest ("An LFO sweeps the target across low..high by default");
        {
            Engine mod;
            mod.prepare (48000.0);

            expect (mod.setModifier (0, drive, "overdrive", "drivePct", lfo (0.0f, 100.0f, 5.0f)));

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

        beginTest ("The knob recentres the sweep, and removal restores that centre");
        {
            Engine mod;
            mod.prepare (48000.0);

            mod.setModifier (0, drive, "overdrive", "drivePct", lfo (0.0f, 100.0f, 5.0f));

            // A fresh modifier centres on the midpoint of low..high.
            float base = 0.0f;
            expect (mod.getBaseValue ("overdrive", "drivePct", base));
            expectWithinAbsoluteError (base, 50.0f, 0.001f);

            // Turning the knob shifts the whole sweep window and its centre.
            expect (mod.setBase ("overdrive", "drivePct", 30.0f));
            expect (mod.getBaseValue ("overdrive", "drivePct", base));
            expectWithinAbsoluteError (base, 30.0f, 0.001f);

            mod.process (0.0f, 512);
            expect (std::abs (drive->get() - 30.0f) > 1.0f, "the modifier did not move the value");

            mod.clearModifier (0);
            mod.process (0.0f, 512); // the audio thread restores on the first inactive block

            expectWithinAbsoluteError (drive->get(), 30.0f, 1.0f);
        }

        beginTest ("The envelope follower opens the sweep with the input level");
        {
            Engine mod;
            mod.prepare (48000.0);

            mod.setModifier (0, drive, "overdrive", "drivePct", lfo (0.0f, 100.0f, 1.0f, Source::envelope));

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

        beginTest ("An expression source follows its CC");
        {
            Engine mod;
            mod.prepare (48000.0);

            float pedal = 0.0f;
            mod.expressionProvider = [&pedal] (int cc) { return cc == 11 ? pedal : 0.0f; };

            Config c;
            c.source = Source::expression;
            c.low = 0.0f;
            c.high = 100.0f;
            c.expressionCc = 11;
            mod.setModifier (0, drive, "overdrive", "drivePct", c);

            pedal = 0.0f;
            mod.process (0.0f, 512);
            const auto heel = drive->get();

            pedal = 1.0f;
            mod.process (0.0f, 512);
            const auto toe = drive->get();

            expect (heel < 10.0f, "pedal heel did not reach the low end: " + juce::String (heel));
            expect (toe > 90.0f, "pedal toe did not reach the high end: " + juce::String (toe));
        }

        beginTest ("A tempo-locked LFO derives its rate from the BPM");
        {
            Engine mod;
            mod.prepare (48000.0);

            Config c;
            c.source = Source::lfoSine;
            c.low = 0.0f;
            c.high = 100.0f;
            c.rateHz = 0.01f; // deliberately tiny: sync must override it
            c.syncDivision = (int) Engine::SyncDivision::quarter;
            mod.setModifier (0, drive, "overdrive", "drivePct", c);

            // 120 BPM, quarter-note cycle -> 2 Hz -> 0.5 s per cycle. Over a full
            // cycle the sine reaches both ends; the tiny free rate, if it were
            // wrongly used, would barely have moved in the same time.
            mod.setBpm (120.0f);

            float lo = 1.0e9f;
            float hi = -1.0e9f;

            for (int b = 0; b < 100; ++b) // 100 * 512 / 48000 = 1.07 s, ~2 cycles
            {
                mod.process (0.0f, 512);
                const auto v = drive->get();
                lo = std::min (lo, v);
                hi = std::max (hi, v);
            }

            expect (hi - lo > 80.0f, "synced LFO barely moved: span " + juce::String (hi - lo));
        }

        beginTest ("The swept value stays inside the target's range");
        {
            Engine mod;
            mod.prepare (48000.0);

            // Deliberately wider than 0..100 so the clamp has to bite.
            mod.setModifier (0, drive, "overdrive", "drivePct", lfo (-50.0f, 200.0f, 5.0f));

            for (int b = 0; b < 20; ++b)
            {
                mod.process (0.0f, 512);
                const auto v = drive->get();
                expect (v >= 0.0f && v <= 100.0f, "value left the range: " + juce::String (v));
            }
        }

        beginTest ("A bad slot or an unsuitable target is refused");
        {
            Engine mod;
            mod.prepare (48000.0);

            const auto* pingPong = registry.findParameter ("delay", "pingPong"); // a switch

            expect (! mod.setModifier (-1, drive, "overdrive", "drivePct", lfo (0, 100, 5)));
            expect (! mod.setModifier (4, drive, "overdrive", "drivePct", lfo (0, 100, 5)));
            expect (! mod.setModifier (0, nullptr, "", "", lfo (0, 100, 5)));

            if (pingPong != nullptr)
                expect (! mod.setModifier (0, pingPong, "delay", "pingPong", lfo (0, 1, 1)),
                        "a boolean switch should not be modulatable");

            expect (! Engine::isValidSlot (-1));
            expect (! Engine::isValidSlot (Engine::kMaxModifiers));
        }

        beginTest ("A modifier reports its binding for the API");
        {
            Engine mod;
            mod.prepare (48000.0);

            mod.setModifier (1, drive, "overdrive", "drivePct", lfo (10.0f, 90.0f, 3.0f, Source::lfoTriangle));

            const auto info = mod.getModifier (1);
            expect (info.active);
            expectEquals (juce::String (info.effectId), juce::String ("overdrive"));
            expectEquals (juce::String (info.parameterId), juce::String ("drivePct"));
            expectEquals (info.source, (int) Source::lfoTriangle);
            expectWithinAbsoluteError (info.low, 10.0f, 0.001f);
            expectWithinAbsoluteError (info.high, 90.0f, 0.001f);
            expectWithinAbsoluteError (info.base, 50.0f, 0.001f);

            mod.clearModifier (1);
            expect (! mod.getModifier (1).active);
        }

        beginTest ("Reports the centre value for persistence while sweeping");
        {
            Engine mod;
            mod.prepare (48000.0);

            mod.setModifier (0, drive, "overdrive", "drivePct", lfo (0.0f, 100.0f, 5.0f));
            mod.setBase ("overdrive", "drivePct", 33.0f);
            mod.process (0.0f, 512); // the live value has moved off 33

            float base = 0.0f;
            expect (mod.getBaseValue ("overdrive", "drivePct", base), "no base reported for a modulated param");
            expectWithinAbsoluteError (base, 33.0f, 0.001f);

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
