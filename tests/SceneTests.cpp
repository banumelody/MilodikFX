#include <JuceHeader.h>

#include "dsp/ChainFactory.h"
#include "dsp/DSPChainManager.h"
#include "preset/PresetManager.h"
#include "preset/SceneManager.h"

namespace
{
juce::File makeTempDirectory (const juce::String& name)
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (name);
    dir.deleteRecursively();
    dir.createDirectory();
    return dir;
}
} // namespace

//==============================================================================
class SceneManagerTests final : public juce::UnitTest
{
public:
    SceneManagerTests() : juce::UnitTest ("SceneManager", "preset") {}

    void runTest() override
    {
        using milodikfx::preset::SceneManager;

        milodikfx::dsp::DSPChainManager manager;
        const auto chain = milodikfx::dsp::buildGuitarChain (manager);

        milodikfx::api::ParameterRegistry registry;
        milodikfx::dsp::registerChainParameters (registry, chain, manager);

        beginTest ("Slots start named and empty");
        {
            SceneManager scenes (registry);

            expectEquals (scenes.getScene (0).name, juce::String ("Clean"));
            expectEquals (scenes.getScene (1).name, juce::String ("Crunch"));
            expectEquals (scenes.getScene (2).name, juce::String ("Lead"));
            expectEquals (scenes.getScene (3).name, juce::String ("Solo"));

            for (int i = 0; i < SceneManager::kNumScenes; ++i)
                expect (! scenes.getScene (i).populated);

            expectEquals (scenes.getActiveIndex(), -1);
        }

        beginTest ("An empty slot recalls nothing rather than switching everything off");
        {
            SceneManager scenes (registry);

            registry.setEffectEnabled ("overdrive", true);

            expect (! scenes.recall (0));
            expect (registry.findEffect ("overdrive")->isEnabled(),
                    "recalling an empty slot changed the chain");
        }

        beginTest ("Capture and recall restore the on/off pattern");
        {
            SceneManager scenes (registry);

            registry.setEffectEnabled ("overdrive", true);
            registry.setEffectEnabled ("delay", false);
            registry.setEffectEnabled ("reverb", true);
            expect (scenes.capture (0));

            registry.setEffectEnabled ("overdrive", false);
            registry.setEffectEnabled ("delay", true);
            registry.setEffectEnabled ("reverb", false);
            expect (scenes.capture (1));

            expect (scenes.recall (0));
            expect (registry.findEffect ("overdrive")->isEnabled());
            expect (! registry.findEffect ("delay")->isEnabled());
            expect (registry.findEffect ("reverb")->isEnabled());
            expectEquals (scenes.getActiveIndex(), 0);

            expect (scenes.recall (1));
            expect (! registry.findEffect ("overdrive")->isEnabled());
            expect (registry.findEffect ("delay")->isEnabled());
            expect (! registry.findEffect ("reverb")->isEnabled());
            expectEquals (scenes.getActiveIndex(), 1);
        }

        beginTest ("A recall never moves a knob");
        {
            // This is the whole design decision: scenes are for switching
            // mid-song, so they must not jump a parameter to a value you cannot
            // see on a control you were not touching.
            SceneManager scenes (registry);

            float applied = 0.0f;
            registry.setParameter ("overdrive", "drivePct", 42.0f, applied);
            registry.setParameter ("delay", "mixPct", 33.0f, applied);
            registry.setEffectEnabled ("overdrive", true);

            scenes.capture (0);

            registry.setParameter ("overdrive", "drivePct", 88.0f, applied);
            registry.setParameter ("delay", "mixPct", 12.0f, applied);
            registry.setEffectEnabled ("overdrive", false);

            scenes.recall (0);

            expect (registry.findEffect ("overdrive")->isEnabled(), "the flag was not restored");
            expectWithinAbsoluteError (registry.findParameter ("overdrive", "drivePct")->get(), 88.0f, 0.01f);
            expectWithinAbsoluteError (registry.findParameter ("delay", "mixPct")->get(), 12.0f, 0.01f);
        }

        beginTest ("Stages that are always in the path are not stored");
        {
            SceneManager scenes (registry);
            scenes.capture (0);

            const auto& stored = scenes.getScene (0).enabled;

            expect (stored.find ("master") == stored.end(), "master should have no scene flag");
            expect (stored.find ("overdrive") != stored.end());
        }

        beginTest ("A slot can be edited without disturbing the chain");
        {
            SceneManager scenes (registry);

            registry.setEffectEnabled ("reverb", true);
            scenes.capture (2);
            scenes.recall (2);
            expectEquals (scenes.getActiveIndex(), 2);

            expect (scenes.setEffectEnabled (2, "reverb", false));

            expect (registry.findEffect ("reverb")->isEnabled(),
                    "editing a slot changed what is playing");

            // ...and the chain no longer matches the slot, so nothing should
            // claim it does.
            expectEquals (scenes.getActiveIndex(), -1);

            expect (scenes.recall (2));
            expect (! registry.findEffect ("reverb")->isEnabled());
        }

        beginTest ("Editing an untouched slot fills in the rest of the chain");
        {
            SceneManager scenes (registry);

            registry.setEffectEnabled ("overdrive", true);
            expect (scenes.setEffectEnabled (3, "reverb", false));

            const auto& scene = scenes.getScene (3);
            expect (scene.populated);
            expect (scene.enabled.size() > 1,
                    "only the edited effect was stored; the rest would be unknown");
            expect (scene.enabled.at ("overdrive"));
            expect (! scene.enabled.at ("reverb"));
        }

        beginTest ("An effect that cannot be bypassed is refused, not silently stored");
        {
            SceneManager scenes (registry);

            expect (! scenes.setEffectEnabled (0, "master", false));
            expect (! scenes.setEffectEnabled (0, "nosuch", false));
            expect (! scenes.setEffectEnabled (0, "", false));
        }

        beginTest ("Slot numbers outside 0-3 are refused");
        {
            SceneManager scenes (registry);

            expect (! scenes.capture (-1));
            expect (! scenes.capture (4));
            expect (! scenes.recall (-1));
            expect (! scenes.recall (99));
            expect (! scenes.setName (7, "Nope"));
            expect (! SceneManager::isValidIndex (-1));
            expect (! SceneManager::isValidIndex (SceneManager::kNumScenes));
        }

        beginTest ("A slot keeps its name and refuses an empty one");
        {
            SceneManager scenes (registry);

            expect (scenes.setName (0, "  Rhythm  "));
            expectEquals (scenes.getScene (0).name, juce::String ("Rhythm"));

            expect (! scenes.setName (0, "   "));
            expectEquals (scenes.getScene (0).name, juce::String ("Rhythm"),
                          "a blank name replaced a good one");
        }

        beginTest ("Slots survive a round trip through JSON");
        {
            SceneManager saved (registry);

            registry.setEffectEnabled ("overdrive", true);
            registry.setEffectEnabled ("delay", false);
            saved.capture (0);
            saved.setName (0, "Rhythm");

            registry.setEffectEnabled ("overdrive", false);
            registry.setEffectEnabled ("delay", true);
            saved.capture (1);

            const auto serialised = juce::JSON::toString (saved.toVar(), true);

            juce::var parsed;
            expect (juce::JSON::parse (serialised, parsed).wasOk());

            SceneManager restored (registry);
            restored.fromVar (parsed);

            expectEquals (restored.getScene (0).name, juce::String ("Rhythm"));
            expect (restored.getScene (0).populated);
            expect (restored.getScene (0).enabled.at ("overdrive"));
            expect (! restored.getScene (0).enabled.at ("delay"));

            expect (restored.getScene (1).populated);
            expect (! restored.getScene (1).enabled.at ("overdrive"));

            expect (! restored.getScene (2).populated);
            expectEquals (restored.getScene (2).name, juce::String ("Lead"));

            // Nothing has been recalled since the load, so no slot may claim to
            // describe what is playing.
            expectEquals (restored.getActiveIndex(), -1);
        }

        beginTest ("Rubbish in the stored slots is ignored, not half-applied");
        {
            SceneManager scenes (registry);
            scenes.capture (0);

            scenes.fromVar (juce::var ("not an array"));
            expect (scenes.getScene (0).populated, "a bad payload wiped a good slot");

            juce::var parsed;
            juce::JSON::parse ("[{\"name\":\"X\"},null,3,{}]", parsed);
            scenes.fromVar (parsed);

            expectEquals (scenes.getScene (0).name, juce::String ("X"));
            expect (! scenes.getScene (0).populated, "a slot with no flags claimed to be populated");
            expectEquals (scenes.getScene (1).name, juce::String ("Crunch"));
        }

        beginTest ("resetToCurrent fills every slot from the chain as it stands");
        {
            SceneManager scenes (registry);

            registry.setEffectEnabled ("overdrive", true);
            scenes.resetToCurrent();

            for (int i = 0; i < SceneManager::kNumScenes; ++i)
            {
                expect (scenes.getScene (i).populated);
                expect (scenes.getScene (i).enabled.at ("overdrive"));
            }

            expectEquals (scenes.getActiveIndex(), -1);
        }
    }
};

static SceneManagerTests sceneManagerTests;

//==============================================================================
class PresetMetadataTests final : public juce::UnitTest
{
public:
    PresetMetadataTests() : juce::UnitTest ("Preset metadata", "preset") {}

    void runTest() override
    {
        using milodikfx::preset::PresetDocument;
        using milodikfx::preset::PresetManager;
        using milodikfx::preset::PresetMetadata;

        auto dir = makeTempDirectory ("MilodikFX_PresetMetadataTests");
        PresetManager manager (dir);

        auto* stateObject = new juce::DynamicObject();
        stateObject->setProperty ("overdrive.drivePct", 50.0);
        const juce::var state (stateObject);

        beginTest ("Metadata survives a save and load");
        {
            PresetDocument document;
            document.state = state;
            document.metadata.description = "Buat lagu pertama";
            document.metadata.tags = { "lead", "crunch" };
            document.metadata.favourite = true;
            document.metadata.notes = "Naikkan mid kalau pakai humbucker";

            expect (manager.saveDocument ("Metadata Test", document));

            PresetDocument loaded;
            expect (manager.loadDocument ("Metadata Test", loaded));

            expectEquals (loaded.metadata.description, juce::String ("Buat lagu pertama"));
            expectEquals (loaded.metadata.tags.size(), 2);
            expect (loaded.metadata.tags.contains ("lead"));
            expect (loaded.metadata.favourite);
            expectEquals (loaded.metadata.notes, juce::String ("Naikkan mid kalau pakai humbucker"));
            expect (loaded.metadata.savedAt.isNotEmpty());
        }

        beginTest ("Re-saving the sound keeps the notes");
        {
            // The regression this exists for: overwriting a preset to change how
            // it sounds must not silently throw away how it was catalogued.
            auto* updatedState = new juce::DynamicObject();
            updatedState->setProperty ("overdrive.drivePct", 90.0);

            expect (manager.savePreset ("Metadata Test", juce::var (updatedState)));

            PresetDocument loaded;
            expect (manager.loadDocument ("Metadata Test", loaded));

            expectEquals (loaded.metadata.notes, juce::String ("Naikkan mid kalau pakai humbucker"));
            expect (loaded.metadata.favourite);
            expectEquals ((double) loaded.state["overdrive.drivePct"], 90.0);
        }

        beginTest ("Metadata can be rewritten without touching the sound");
        {
            PresetMetadata metadata;
            metadata.description = "Diperbarui";
            metadata.favourite = false;

            expect (manager.updateMetadata ("Metadata Test", metadata));

            PresetDocument loaded;
            expect (manager.loadDocument ("Metadata Test", loaded));

            expectEquals (loaded.metadata.description, juce::String ("Diperbarui"));
            expect (! loaded.metadata.favourite);
            expectEquals ((double) loaded.state["overdrive.drivePct"], 90.0);
        }

        beginTest ("Metadata on a preset that is not there fails rather than creating one");
        {
            PresetMetadata metadata;
            metadata.description = "Hantu";

            expect (! manager.updateMetadata ("No Such Preset", metadata));
            expect (! manager.presetExists ("No Such Preset"));
        }

        beginTest ("A version 2 file still loads, just without the new fields");
        {
            // Presets written before metadata existed have to keep working.
            const juce::String legacy = R"({
                "schemaVersion": 2,
                "name": "Legacy",
                "savedAt": "2026-01-01T00:00:00+0000",
                "state": { "overdrive.drivePct": 25.0 }
            })";

            dir.getChildFile ("Legacy.json").replaceWithText (legacy);

            PresetDocument loaded;
            expect (manager.loadDocument ("Legacy", loaded));

            expectEquals ((double) loaded.state["overdrive.drivePct"], 25.0);
            expect (loaded.metadata.description.isEmpty());
            expect (loaded.metadata.tags.isEmpty());
            expect (! loaded.metadata.favourite);
            expect (! loaded.scenes.isArray());
        }

        beginTest ("Blank and duplicate tags are dropped");
        {
            PresetDocument document;
            document.state = state;
            document.metadata.tags = { "lead", "  ", "", "  lead  " };

            expect (manager.saveDocument ("Tag Test", document));

            PresetDocument loaded;
            expect (manager.loadDocument ("Tag Test", loaded));

            expectEquals (loaded.metadata.tags.size(), 1);
            expectEquals (loaded.metadata.tags[0], juce::String ("lead"));
        }

        beginTest ("Scenes travel inside the preset");
        {
            juce::var scenes;
            juce::JSON::parse (R"([{"name":"Rhythm","populated":true,"enabled":{"overdrive":true}}])", scenes);

            PresetDocument document;
            document.state = state;
            document.scenes = scenes;

            expect (manager.saveDocument ("Scene Test", document));

            PresetDocument loaded;
            expect (manager.loadDocument ("Scene Test", loaded));

            expect (loaded.scenes.isArray());
            expectEquals (loaded.scenes[0]["name"].toString(), juce::String ("Rhythm"));
        }

        beginTest ("Export gives back the file, import puts it back");
        {
            const auto exported = manager.exportPreset ("Metadata Test");
            expect (exported.isNotEmpty());
            expect (exported.contains ("Diperbarui"));

            const auto stored = manager.importPreset ("Imported Copy", exported);
            expectEquals (stored, juce::String ("Imported Copy"));

            PresetDocument loaded;
            expect (manager.loadDocument ("Imported Copy", loaded));
            expectEquals (loaded.metadata.description, juce::String ("Diperbarui"));
            expectEquals ((double) loaded.state["overdrive.drivePct"], 90.0);
        }

        beginTest ("Exporting something that is not there gives nothing");
        {
            expect (manager.exportPreset ("No Such Preset").isEmpty());
            expect (manager.exportPreset ("").isEmpty());
        }

        beginTest ("A file that is not a preset is refused");
        {
            // Otherwise the library gains an entry that loads into silence.
            expect (manager.importPreset ("Bad", "not json at all").isEmpty());
            expect (manager.importPreset ("Bad", "[1,2,3]").isEmpty());
            expect (manager.importPreset ("Bad", R"({"name":"Bad"})").isEmpty());
            expect (manager.importPreset ("Bad", R"({"state":"not an object"})").isEmpty());

            expect (! manager.presetExists ("Bad"));
        }

        beginTest ("An import falls back to the name inside the file");
        {
            const auto exported = manager.exportPreset ("Metadata Test");

            const auto stored = manager.importPreset ("", exported);
            expectEquals (stored, juce::String ("Metadata Test"));
        }

        beginTest ("An import cannot write outside the preset directory");
        {
            const auto exported = manager.exportPreset ("Metadata Test");

            const auto stored = manager.importPreset ("../../evil", exported);

            // Whatever it is called, it has to be inside the library.
            if (stored.isNotEmpty())
            {
                expect (! stored.contains (".."));
                expect (! stored.contains ("/") && ! stored.contains ("\\"));
                expect (manager.presetExists (stored));
            }

            expect (! dir.getParentDirectory().getChildFile ("evil.json").existsAsFile());
        }

        dir.deleteRecursively();
    }
};

static PresetMetadataTests presetMetadataTests;
