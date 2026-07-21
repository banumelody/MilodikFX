#include <JuceHeader.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>

#include "api/ParameterRegistry.h"
#include "dsp/ChainFactory.h"
#include "preset/PresetManager.h"

namespace
{
struct FakeEffect
{
    bool enabled = true;
    float drive = 0.0f;
    float level = 100.0f;
};

milodikfx::api::EffectDescriptor makeFakeDescriptor (FakeEffect& target)
{
    milodikfx::api::EffectDescriptor e;
    e.id = "overdrive";
    e.label = "Overdrive";
    e.isEnabled = [&target] { return target.enabled; };
    e.setEnabled = [&target] (bool v) { target.enabled = v; };

    milodikfx::api::ParameterDescriptor drive;
    drive.id = "drivePct";
    drive.label = "Drive";
    drive.unit = "%";
    drive.minValue = 0.0f;
    drive.maxValue = 100.0f;
    drive.defaultValue = 0.0f;
    drive.get = [&target] { return target.drive; };
    drive.set = [&target] (float v) { target.drive = v; };
    e.parameters.push_back (drive);

    milodikfx::api::ParameterDescriptor level;
    level.id = "levelPct";
    level.label = "Level";
    level.unit = "%";
    level.minValue = 0.0f;
    level.maxValue = 100.0f;
    level.defaultValue = 100.0f;
    level.get = [&target] { return target.level; };
    level.set = [&target] (float v) { target.level = v; };
    e.parameters.push_back (level);

    return e;
}
} // namespace

class ParameterRegistryTests final : public juce::UnitTest
{
public:
    ParameterRegistryTests() : juce::UnitTest ("ParameterRegistry") {}

    void runTest() override
    {
        beginTest ("Lookup is case insensitive");

        FakeEffect fake;
        milodikfx::api::ParameterRegistry registry;
        registry.addEffect (makeFakeDescriptor (fake));

        expect (registry.findEffect ("overdrive") != nullptr);
        expect (registry.findEffect ("OverDrive") != nullptr);
        expect (registry.findEffect ("OVERDRIVE") != nullptr);
        expect (registry.findEffect ("chorus") == nullptr);
        expect (registry.findParameter ("OVERDRIVE", "DRIVEPCT") != nullptr);

        beginTest ("Values are clamped to the descriptor range");

        float applied = 0.0f;

        expect (registry.setParameter ("overdrive", "drivePct", 250.0f, applied));
        expect (std::abs (applied - 100.0f) < 0.001f);
        expect (std::abs (fake.drive - 100.0f) < 0.001f);

        expect (registry.setParameter ("overdrive", "drivePct", -50.0f, applied));
        expect (std::abs (applied - 0.0f) < 0.001f);

        beginTest ("Non-finite values are rejected");

        fake.drive = 42.0f;
        expect (! registry.setParameter ("overdrive", "drivePct",
                                         std::numeric_limits<float>::quiet_NaN(), applied));
        expect (std::abs (fake.drive - 42.0f) < 0.001f);

        expect (! registry.setParameter ("overdrive", "nonexistent", 1.0f, applied));
        expect (! registry.setParameter ("nonexistent", "drivePct", 1.0f, applied));

        beginTest ("Change callback fires on every successful mutation");

        auto changes = 0;
        registry.onChanged = [&changes] { ++changes; };

        registry.setParameter ("overdrive", "drivePct", 10.0f, applied);
        registry.setEffectEnabled ("overdrive", false);
        registry.setParameter ("overdrive", "unknown", 10.0f, applied);

        expectEquals (changes, 2);
        expect (! fake.enabled);

        beginTest ("captureState / applyState round trip");

        registry.setEffectEnabled ("overdrive", true);
        registry.setParameter ("overdrive", "drivePct", 63.5f, applied);
        registry.setParameter ("overdrive", "levelPct", 22.0f, applied);

        const auto snapshot = registry.captureState();

        registry.setEffectEnabled ("overdrive", false);
        registry.setParameter ("overdrive", "drivePct", 0.0f, applied);
        registry.setParameter ("overdrive", "levelPct", 100.0f, applied);

        const auto count = registry.applyState (snapshot);

        expect (count >= 3);
        expect (fake.enabled);
        expect (std::abs (fake.drive - 63.5f) < 0.001f);
        expect (std::abs (fake.level - 22.0f) < 0.001f);

        beginTest ("applyState ignores unknown ids and junk");

        const auto before = fake.drive;
        expectEquals (registry.applyState (juce::var()), 0);
        expectEquals (registry.applyState (juce::var ("not an object")), 0);
        expect (std::abs (fake.drive - before) < 0.001f);

        beginTest ("toVar exposes range metadata for the UI");

        const auto state = registry.toVar();
        const auto effects = state["effects"];

        expect (effects.isArray());
        expectEquals (effects.size(), 1);

        const auto first = effects[0];
        expectEquals (first["id"].toString(), juce::String ("overdrive"));

        const auto parameters = first["parameters"];
        expect (parameters.isArray());
        expectEquals (parameters.size(), 2);

        const auto driveVar = parameters[0];
        expectEquals (driveVar["id"].toString(), juce::String ("drivePct"));
        expectEquals (driveVar["unit"].toString(), juce::String ("%"));
        expectEquals (driveVar["type"].toString(), juce::String ("float"));
        expect (std::abs ((double) driveVar["max"] - 100.0) < 0.001);

        beginTest ("Serialised state survives a JSON round trip");

        const auto json = juce::JSON::toString (registry.captureState(), false);

        juce::var reparsed;
        expect (juce::JSON::parse (json, reparsed).wasOk());

        registry.setParameter ("overdrive", "drivePct", 1.0f, applied);
        expect (registry.applyState (reparsed) >= 3);
        expect (std::abs (fake.drive - 63.5f) < 0.001f);
    }
};

static ParameterRegistryTests parameterRegistryTests;

//==============================================================================
class PresetManagerTests final : public juce::UnitTest
{
public:
    PresetManagerTests() : juce::UnitTest ("PresetManager") {}

    void runTest() override
    {
        auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getNonexistentChildFile ("MilodikFXPresetsTest", "", false);
        tempDir.createDirectory();

        milodikfx::preset::PresetManager manager (tempDir);

        beginTest ("Save / list / load / delete round trip");

        FakeEffect fake;
        milodikfx::api::ParameterRegistry registry;
        registry.addEffect (makeFakeDescriptor (fake));

        float applied = 0.0f;
        registry.setParameter ("overdrive", "drivePct", 77.0f, applied);
        registry.setParameter ("overdrive", "levelPct", 42.0f, applied);
        registry.setEffectEnabled ("overdrive", false);

        expect (manager.savePreset ("My Preset", registry.captureState()));
        expect (manager.listPresets().contains ("My Preset"));
        expect (manager.presetExists ("My Preset"));

        registry.setParameter ("overdrive", "drivePct", 0.0f, applied);
        registry.setEffectEnabled ("overdrive", true);

        juce::var loaded;
        expect (manager.loadPreset ("My Preset", loaded));
        expect (registry.applyState (loaded) >= 3);

        expect (std::abs (fake.drive - 77.0f) < 0.001f);
        expect (std::abs (fake.level - 42.0f) < 0.001f);
        expect (! fake.enabled);

        expect (manager.deletePreset ("My Preset"));
        expect (! manager.listPresets().contains ("My Preset"));
        expect (! manager.loadPreset ("My Preset", loaded));

        beginTest ("A name with quotes survives the JSON round trip");

        // Response and file bodies used to be concatenated by hand, so a quote
        // in a preset name produced a broken document.
        const juce::String awkward = "Lead \"Hot\" \\ Tone";
        const auto safeName = milodikfx::preset::PresetManager::sanitisePresetName (awkward);

        expect (safeName.isNotEmpty());
        expect (manager.savePreset (awkward, registry.captureState()));
        expect (manager.listPresets().contains (safeName));

        juce::var awkwardLoaded;
        expect (manager.loadPreset (awkward, awkwardLoaded));
        expect (awkwardLoaded.isObject());

        beginTest ("Names cannot escape the presets directory");

        for (const auto* attempt : { "../evil", "..\\evil", "C:/Windows/evil", "sub/dir/evil" })
        {
            const auto name = juce::String (attempt);
            manager.savePreset (name, registry.captureState());

            const auto sanitised = milodikfx::preset::PresetManager::sanitisePresetName (name);
            expect (! sanitised.containsChar ('/'), "slash survived: " + sanitised);
            expect (! sanitised.containsChar ('\\'), "backslash survived: " + sanitised);
            expect (! sanitised.contains (".."), "traversal survived: " + sanitised);
        }

        // Everything written must live directly inside the presets directory.
        juce::Array<juce::File> escaped;
        tempDir.getParentDirectory().findChildFiles (escaped, juce::File::findFiles, false, "evil*");
        expect (escaped.isEmpty(), "a preset was written outside the presets directory");

        beginTest ("An empty name is rejected");

        expect (! manager.savePreset ("   ", registry.captureState()));
        expect (! manager.loadPreset ("", loaded));

        beginTest ("ensurePresetExists only writes when missing");

        expect (manager.ensurePresetExists ("Default Clean", registry.captureState()));
        const auto firstWrite = tempDir.getChildFile ("Default Clean.json").getLastModificationTime();
        expect (manager.ensurePresetExists ("Default Clean", registry.captureState()));
        expect (tempDir.getChildFile ("Default Clean.json").getLastModificationTime() == firstWrite);

        tempDir.deleteRecursively();
    }
};

static PresetManagerTests presetManagerTests;

//==============================================================================
class ChainFactoryTests final : public juce::UnitTest
{
public:
    ChainFactoryTests() : juce::UnitTest ("ChainFactory") {}

    void runTest() override
    {
        beginTest ("The chain is built in the documented order");

        milodikfx::dsp::DSPChainManager manager;
        const auto chain = milodikfx::dsp::buildGuitarChain (manager);

        // Eleven stages the guitar passes through. The metronome is deliberately
        // not one of them -- it is mixed in afterwards, outside bypass.
        expectEquals (manager.getNumProcessors(), 11);
        expect (chain.metronome != nullptr);

        // Ahead of the gate, so the gate threshold tracks the trim rather than
        // being tied to raw interface level.
        expect (chain.inputTrim != nullptr);
        expect (chain.noiseGate != nullptr);
        expect (chain.cleanBoost != nullptr);
        expect (chain.compressor != nullptr);
        expect (chain.overdrive != nullptr);
        expect (chain.eq != nullptr);
        expect (chain.toneStack != nullptr);
        expect (chain.cabinet != nullptr);
        expect (chain.delay != nullptr);
        expect (chain.reverb != nullptr);
        expect (chain.masterOut != nullptr);

        beginTest ("The plugin and the app share one parameter set");

        // Both builds get the Input card, because the trim is a chain stage and
        // a plugin needs to match the guitar to the chain just as much. Only
        // channel routing is host-specific -- the app maps device channels
        // itself, a plugin gets whatever the host sends -- so Mode is the one
        // parameter that differs. Everything else must match exactly.
        milodikfx::api::ParameterRegistry pluginRegistry;
        milodikfx::dsp::registerChainParameters (pluginRegistry, chain, manager);

        milodikfx::api::ParameterRegistry appRegistry;
        milodikfx::dsp::ChainExtras appExtras;
        appExtras.getInputMode = [] { return 0.0f; };
        appExtras.setInputMode = [] (float) {};

        milodikfx::dsp::registerChainParameters (appRegistry, chain, manager, std::move (appExtras));

        expectEquals ((int) pluginRegistry.getEffects().size(), 13);
        expectEquals ((int) appRegistry.getEffects().size(), 13);

        // The plugin has the Input card too, with the trim but without Mode.
        const auto* pluginInput = pluginRegistry.findEffect ("input");
        const auto* appInput = appRegistry.findEffect ("input");

        expect (pluginInput != nullptr && appInput != nullptr);

        if (pluginInput != nullptr && appInput != nullptr)
        {
            expect (pluginRegistry.findParameter ("input", "gainDb") != nullptr);
            expect (appRegistry.findParameter ("input", "gainDb") != nullptr);

            expect (pluginRegistry.findParameter ("input", "mode") == nullptr,
                    "a plugin has no device channels to route");
            expect (appRegistry.findParameter ("input", "mode") != nullptr);

            expectEquals ((int) appInput->parameters.size(),
                          (int) pluginInput->parameters.size() + 1);
        }

        for (const auto& effect : pluginRegistry.getEffects())
        {
            const auto* twin = appRegistry.findEffect (effect.id);
            expect (twin != nullptr, "app registry is missing " + juce::String (effect.id));

            // Input is the one card allowed to differ, and only by Mode.
            if (twin != nullptr && effect.id != "input")
                expectEquals ((int) twin->parameters.size(), (int) effect.parameters.size(),
                              juce::String (effect.id) + " has a different parameter count");
        }

        beginTest ("Stages that are always in the path cannot be bypassed");

        // Master carried an on/off switch that mapped to mute. In the UI it was
        // indistinguishable from an effect bypass, so switching it silenced the
        // whole app with nothing to say why. Mute is now an explicit parameter.
        const auto* master = appRegistry.findEffect ("master");
        expect (master != nullptr);

        if (master != nullptr)
        {
            expect (master->setEnabled == nullptr, "master must not be toggleable");
            expect (master->isEnabled && master->isEnabled(), "master must always report enabled");

            auto foundMute = false;
            for (const auto& parameter : master->parameters)
                if (parameter.id == "muted")
                    foundMute = true;

            expect (foundMute, "master must expose an explicit mute parameter");
        }

        const auto* input = appRegistry.findEffect ("input");
        expect (input != nullptr);

        if (input != nullptr)
            expect (input->setEnabled == nullptr, "input routing must not be toggleable");

        float ignored = 0.0f;
        expect (! appRegistry.setEffectEnabled ("master", false), "master bypass must be refused");
        expect (! appRegistry.setEffectEnabled ("input", false), "input bypass must be refused");
        expect (appRegistry.setParameter ("master", "muted", 1.0f, ignored), "mute must be settable");
        expect (chain.masterOut->isMuted());
        appRegistry.setParameter ("master", "muted", 0.0f, ignored);
        expect (! chain.masterOut->isMuted());

        const auto* global = appRegistry.findEffect ("global");
        expect (global != nullptr);

        if (global != nullptr)
        {
            expect (global->setEnabled == nullptr, "global stage must not be toggleable");

            auto foundBypass = false;
            for (const auto& parameter : global->parameters)
                if (parameter.id == "bypass")
                    foundBypass = true;

            expect (foundBypass, "global stage must expose a bypass parameter");
        }

        // Every other effect must still be bypassable.
        for (const auto& effect : appRegistry.getEffects())
            if (effect.id != "master" && effect.id != "input" && effect.id != "global")
                expect (effect.setEnabled != nullptr,
                        juce::String (effect.id) + " should be toggleable");

        beginTest ("Every parameter is readable, writable and sanely bounded");

        auto parameterCount = 0;

        appRegistry.forEachParameter (
            [this, &parameterCount] (const milodikfx::api::EffectDescriptor& effect,
                                     const milodikfx::api::ParameterDescriptor& parameter)
            {
                ++parameterCount;

                const auto where = juce::String (effect.id) + "." + juce::String (parameter.id);

                expect (parameter.get != nullptr, where + " has no getter");
                expect (parameter.set != nullptr, where + " has no setter");
                expect (parameter.maxValue > parameter.minValue, where + " has an empty range");
                expect (parameter.defaultValue >= parameter.minValue
                            && parameter.defaultValue <= parameter.maxValue,
                        where + " default sits outside its range");
                expect (! parameter.label.empty(), where + " has no label");
            });

        expect (parameterCount >= 30, "only " + juce::String (parameterCount) + " parameters registered");

        beginTest ("Writing the extremes of every parameter never breaks the chain");

        milodikfx::dsp::DSPChainManager liveManager;
        const auto liveChain = milodikfx::dsp::buildGuitarChain (liveManager);

        milodikfx::api::ParameterRegistry liveRegistry;
        milodikfx::dsp::registerChainParameters (liveRegistry, liveChain, liveManager);

        liveManager.prepareToPlay (48000.0, 512, 2);

        // Enable everything, including the blocks that ship off, so nothing is
        // skipped by an early return.
        for (const auto& effect : liveRegistry.getEffects())
            if (effect.setEnabled)
                effect.setEnabled (true);

        const float extremes[] = { 0.0f, 0.5f, 1.0f };

        for (const auto position : extremes)
        {
            liveRegistry.forEachParameter (
                [position] (const milodikfx::api::EffectDescriptor&,
                            const milodikfx::api::ParameterDescriptor& parameter)
                {
                    parameter.set (parameter.minValue
                                   + position * (parameter.maxValue - parameter.minValue));
                });

            juce::AudioBuffer<float> buffer (2, 512);

            for (int block = 0; block < 6; ++block)
            {
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 512; ++i)
                        buffer.setSample (ch, i, 0.8f * std::sin (0.05 * (block * 512 + i)));

                liveManager.processBlock (buffer);

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 512; ++i)
                    {
                        const auto sample = buffer.getSample (ch, i);

                        expect (std::isfinite (sample),
                                "non-finite output at extreme " + juce::String (position));
                        expect (std::abs (sample) <= 1.0f,
                                "output exceeded full scale at extreme " + juce::String (position)
                                    + ": " + juce::String (sample));
                    }
            }
        }
    }
};

static ChainFactoryTests chainFactoryTests;

//==============================================================================
/**
 * Reproduces the shape of a real crash: a device change resizes every delay
 * line and filter-state vector from the message thread while a block is already
 * in flight on the audio thread. The engine caught a "vector subscript out of
 * range" from DelayProcessor::processBlock doing exactly this.
 *
 * The audio thread must survive a resize underneath it, so nothing on that side
 * may derive an index from anything but the container it is about to touch.
 */
class ConcurrentPrepareTests final : public juce::UnitTest
{
public:
    ConcurrentPrepareTests() : juce::UnitTest ("Concurrent prepare") {}

    void runTest() override
    {
        beginTest ("Processing survives prepareToPlay running underneath it");

        milodikfx::dsp::DSPChainManager manager;
        const auto chain = milodikfx::dsp::buildGuitarChain (manager);

        milodikfx::api::ParameterRegistry registry;
        milodikfx::dsp::registerChainParameters (registry, chain, manager);

        // Everything on, including the blocks that ship disabled, so no stage
        // can be skipped by an early return.
        for (const auto& effect : registry.getEffects())
            if (effect.setEnabled)
                effect.setEnabled (true);

        // The longest delay line exercises the largest indices.
        chain.delay->setTimeMs (1000.0f);
        chain.delay->setFeedbackPercent (60.0f);
        chain.delay->setMixPercent (50.0f);

        manager.prepareToPlay (48000.0, 1024, 2);

        std::atomic<bool> stop { false };
        std::atomic<int> blocksProcessed { 0 };
        std::atomic<bool> sawNonFinite { false };

        std::thread audioThread ([&]
        {
            juce::AudioBuffer<float> block (2, 256);

            while (! stop.load())
            {
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 256; ++i)
                        block.setSample (ch, i, 0.5f * std::sin (0.01f * (float) i));

                manager.processBlock (block);

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 256; ++i)
                        if (! std::isfinite (block.getSample (ch, i)))
                            sawNonFinite.store (true);

                blocksProcessed.fetch_add (1);
            }
        });

        const double rates[] = { 96000.0, 44100.0, 192000.0, 48000.0 };

        for (int round = 0; round < 6; ++round)
        {
            for (const auto rate : rates)
            {
                manager.prepareToPlay (rate, 1024, 2);
                std::this_thread::sleep_for (std::chrono::milliseconds (4));
            }
        }

        stop.store (true);
        audioThread.join();

        logMessage ("processed " + juce::String (blocksProcessed.load()) + " blocks across 24 device changes");

        expect (blocksProcessed.load() > 0, "the audio thread never ran");
        expect (! sawNonFinite.load(), "a non-finite sample reached the output");
    }
};

static ConcurrentPrepareTests concurrentPrepareTests;
