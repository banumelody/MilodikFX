#include <JuceHeader.h>

#include <algorithm>
#include <string>
#include <vector>

#include "api/ParameterRegistry.h"
#include "api/PresetsHandler.h"
#include "dsp/ChainFactory.h"
#include "dsp/ChainOrder.h"
#include "dsp/DSPChainManager.h"
#include "preset/ChannelStore.h"
#include "preset/PresetManager.h"
#include "preset/SceneManager.h"

namespace
{
bool contains (const std::vector<std::string>& ids, const std::string& id)
{
    return std::find (ids.begin(), ids.end(), id) != ids.end();
}

/** A chain plus everything a PresetsHandler needs to save and load one. */
struct Rig
{
    explicit Rig (const juce::File& root)
        : chain (milodikfx::dsp::buildGuitarChain (manager, { false, false })),
          order (chain, manager),
          presetManager (root),
          scenes (registry),
          channels (registry)
    {
        milodikfx::dsp::registerChainParameters (registry, chain, manager);
        scenes.setChannelStore (&channels);
        channels.resetToCurrent();
    }

    /** Wires a handler the way the app does -- with the chain order attached. */
    std::unique_ptr<PresetsHandler> handler (bool withChainOrder)
    {
        auto h = std::make_unique<PresetsHandler> (presetManager, registry);
        h->setSceneManager (&scenes);
        h->setChannelStore (&channels);

        if (withChainOrder)
            h->setChainOrder (&order);

        return h;
    }

    milodikfx::dsp::DSPChainManager manager;
    milodikfx::dsp::GuitarChain chain;
    milodikfx::dsp::ChainOrder order;
    milodikfx::api::ParameterRegistry registry;
    milodikfx::preset::PresetManager presetManager;
    milodikfx::preset::SceneManager scenes;
    milodikfx::preset::ChannelStore channels;
};

/**
 * The chain surface in a plugin.
 *
 * `PluginBackend` never gave its `PresetsHandler` a `ChainOrder`, so a preset's
 * stage order, bus assignment and board were ignored on load and left out on
 * save. That is not a missing feature -- it is a preset built in the app
 * sounding different in the DAW, and one saved in the DAW losing its
 * arrangement on the way back. These lock both directions.
 */
class PluginChainTests final : public juce::UnitTest
{
public:
    PluginChainTests() : juce::UnitTest ("Plugin chain surface", "milodikfx") {}

    void runTest() override
    {
        const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("MilodikFXPluginChainTests");

        root.deleteRecursively();
        root.createDirectory();

        beginTest ("A preset carries the order, buses and board when one is attached");
        {
            Rig rig (root);
            auto saver = rig.handler (true);

            rig.order.setPlacedIds ({ "overdrive", "delay" });
            rig.order.setBusBIds ({ "delay" });

            const auto saved = saver->handlePost ("/api/presets", R"({"name":"arranged"})");
            expectEquals (saved.statusCode, 200);

            const auto file = root.getChildFile ("arranged.json");
            expect (file.existsAsFile());

            const auto parsed = juce::JSON::parse (file.loadFileAsString());

            expect (parsed["chainOrder"].isArray(), "the order was not written");
            expect (parsed["chainBusB"].isArray(), "the bus assignment was not written");
            expect (parsed["chainBoard"].isArray(), "the board was not written");
        }

        beginTest ("Without one, all three are silently dropped");
        {
            // This is exactly what the plugin did. Kept as a test because the
            // failure is invisible: the preset saves happily and loses data.
            Rig rig (root);
            auto saver = rig.handler (false);

            rig.order.setPlacedIds ({ "overdrive" });

            expectEquals (saver->handlePost ("/api/presets", R"({"name":"bare"})").statusCode, 200);

            const auto parsed =
                juce::JSON::parse (root.getChildFile ("bare.json").loadFileAsString());

            expect (! parsed["chainOrder"].isArray());
            expect (! parsed["chainBoard"].isArray());
        }

        beginTest ("Loading applies the order, buses and board");
        {
            Rig source (root);
            auto saver = source.handler (true);

            source.order.setPlacedIds ({ "overdrive", "reverb" });
            source.order.setBusBIds ({ "reverb" });
            expectEquals (saver->handlePost ("/api/presets", R"({"name":"shared"})").statusCode, 200);

            // A second rig standing in for the plugin: same chain, nothing set.
            Rig target (root);
            auto loader = target.handler (true);

            expect (contains (target.order.getPlacedIds(), "cabinet"),
                    "the target should start with a full board");

            expectEquals (loader->handlePost ("/api/presets/load", R"({"name":"shared"})").statusCode, 200);

            const auto placed = target.order.getPlacedIds();

            expect (contains (placed, "overdrive"));
            expect (contains (placed, "reverb"));
            expect (! contains (placed, "cabinet"), "the board did not travel with the preset");
            expect (contains (target.order.getBusBIds(), "reverb"),
                    "the bus assignment did not travel with the preset");
        }

        beginTest ("A preset with no arrangement still loads as a full board");
        {
            // The migration rule, which the plugin has to honour too: absent
            // means "as built" and "all placed", never an empty board.
            Rig rig (root);
            auto handler = rig.handler (true);

            expectEquals (handler->handlePost ("/api/presets", R"({"name":"plain"})").statusCode, 200);

            const auto file = root.getChildFile ("plain.json");
            auto parsed = juce::JSON::parse (file.loadFileAsString());

            if (auto* object = parsed.getDynamicObject())
            {
                object->removeProperty ("chainOrder");
                object->removeProperty ("chainBusB");
                object->removeProperty ("chainBoard");
            }

            file.replaceWithText (juce::JSON::toString (parsed, true));

            rig.order.setPlacedIds ({ "overdrive" });
            expectEquals (handler->handlePost ("/api/presets/load", R"({"name":"plain"})").statusCode, 200);

            expectEquals ((int) rig.order.getPlacedIds().size(),
                          (int) rig.order.getStageIds().size());
        }

        beginTest ("The plugin's chain has the same stages the app's does");
        {
            // The plugin drops the looper and the metronome, which are
            // post-master. The chain itself must be identical, or a preset's
            // stage ids would not mean the same thing in both.
            milodikfx::dsp::DSPChainManager appManager;
            const auto appChain = milodikfx::dsp::buildGuitarChain (appManager);
            milodikfx::dsp::ChainOrder appOrder (appChain, appManager);

            milodikfx::dsp::DSPChainManager hostManager;
            const auto hostChain = milodikfx::dsp::buildGuitarChain (hostManager, { false, false });
            milodikfx::dsp::ChainOrder hostOrder (hostChain, hostManager);

            expect (appOrder.getStageIds() == hostOrder.getStageIds(),
                    "the app and the plugin disagree about what the chain contains");
        }

        root.deleteRecursively();
    }
};

static PluginChainTests pluginChainTests;
} // namespace
