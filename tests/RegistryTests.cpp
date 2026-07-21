#include <JuceHeader.h>

#include "api/ParameterRegistry.h"
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
