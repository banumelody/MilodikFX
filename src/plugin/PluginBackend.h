#pragma once

#include <JuceHeader.h>

#include <memory>

#include "api/HttpHandler.h"
#include "api/LevelsHandler.h"
#include "api/ParameterRegistry.h"
#include "api/RestApiDispatcher.h"
#include "dsp/ChainOrder.h"
#include "preset/ChannelStore.h"
#include "preset/PinnedControls.h"
#include "preset/PresetManager.h"
#include "preset/SceneManager.h"

namespace milodikfx::plugin
{
class MilodikFXAudioProcessor;

/**
 * The plugin's API surface — the same one the standalone app serves over HTTP.
 *
 * `RestApiDispatcher` and `HttpHandler` never knew they were talking to a
 * socket: they take a method, a path, a query and a body, and hand back a status
 * and a body. That is the whole reason the plugin can host the identical React
 * UI without becoming a web server. A plugin instance must not open a port --
 * several instances in one project would fight over it, and a DAW has no
 * business hosting a server -- so the editor's WebView calls straight into this
 * through JUCE's native function bridge instead.
 *
 * What the plugin deliberately does *not* register:
 *
 * - **devices** and **midi**: the host owns both. Offering an audio-device
 *   picker inside a DAW would be, at best, a lie.
 * - **looper** and **metronome**: not built into the plugin's chain at all.
 * - **update**: a plugin has no business reaching out to GitHub.
 *
 * Everything else -- effects, parameters, presets, scenes, channels, pins,
 * impulse responses, amp models, meters, the tuner -- is the same code the app
 * runs, so the two cannot drift.
 *
 * Threading: every call arrives on the message thread, which is where the
 * WebView delivers native function invocations. That is the same contract the
 * app's handlers already rely on for anything that touches a file.
 */
class PluginBackend
{
public:
    explicit PluginBackend (MilodikFXAudioProcessor& processor);

    /** Dispatches one request. Message thread only. */
    HttpHandler::Response call (const juce::String& method,
                                const juce::String& path,
                                const juce::String& query,
                                const juce::String& body);

    LevelsHandler& getLevels() noexcept { return *levelsHandler; }

    milodikfx::preset::SceneManager& getScenes() noexcept { return sceneManager; }
    milodikfx::preset::ChannelStore& getChannels() noexcept { return channelStore; }
    milodikfx::preset::PinnedControls& getPins() noexcept { return pinnedControls; }

    /** Snapshot of the scene, channel and pin state, for the plugin's own state blob. */
    juce::var captureLayout() const;
    void applyLayout (const juce::var& layout);

private:
    juce::var captureModifiers() const;
    void applyModifiers (const juce::var& value);

    MilodikFXAudioProcessor& processor;

    milodikfx::preset::SceneManager sceneManager;
    milodikfx::preset::ChannelStore channelStore;
    milodikfx::preset::PinnedControls pinnedControls;

    /**
     * Translates the chain's order, bus assignment and board between ids and
     * processor indices.
     *
     * The plugin went without one until v0.34, which meant `PresetsHandler` had
     * nothing to apply a preset's `chainOrder`, `chainBusB` or `chainBoard` to --
     * so they were ignored on load and omitted on save. A preset built in the
     * app sounded different here, and one saved here lost its arrangement.
     */
    milodikfx::dsp::ChainOrder chainOrder;

    std::shared_ptr<LevelsHandler> levelsHandler;
    RestApiDispatcher dispatcher;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginBackend)
};
} // namespace milodikfx::plugin
