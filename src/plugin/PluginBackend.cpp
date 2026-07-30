#include "plugin/PluginBackend.h"

#include "api/EffectsHandler.h"
#include "api/HealthHandler.h"
#include "api/IrHandler.h"
#include "api/NamHandler.h"
#include "api/ParametersHandler.h"
#include "api/PinsHandler.h"
#include "api/PresetsHandler.h"
#include "api/ScenesHandler.h"
#include "plugin/PluginProcessor.h"

namespace milodikfx::plugin
{
namespace
{
const juce::Identifier kScenesProperty { "scenes" };
const juce::Identifier kChannelsProperty { "channels" };
const juce::Identifier kPinsProperty { "pins" };
} // namespace

PluginBackend::PluginBackend (MilodikFXAudioProcessor& processorToUse)
    : processor (processorToUse),
      sceneManager (processorToUse.getRegistry()),
      channelStore (processorToUse.getRegistry())
{
    auto& registry = processor.getRegistry();

    sceneManager.setChannelStore (&channelStore);
    channelStore.resetToCurrent();

    levelsHandler = std::make_shared<LevelsHandler>();

    auto effects = std::make_shared<EffectsHandler> (registry);
    effects->setChannelStore (&channelStore);

    auto presets = std::make_shared<PresetsHandler> (processor.getPresetManager(), registry);
    presets->setSceneManager (&sceneManager);
    presets->setChannelStore (&channelStore);
    presets->setPinnedControls (&pinnedControls);

    // The host owns persistence, so a change does not write a settings file --
    // it marks the processor's state dirty, and the host asks for it when it
    // saves the project. Told through updateHostDisplay so a DAW notices the
    // project has unsaved changes at all.
    const auto touched = [this] { processor.markStateChanged(); };

    presets->onSelectionChanged = [this, touched] (const juce::String& name)
    {
        processor.setCurrentPresetName (name);

        // Which preset is loaded is part of the state the host saves, and it is
        // not a parameter -- without this the project would reopen on whatever
        // preset it had before.
        touched();

        // A preset load moved the chain underneath the host's parameter values.
        processor.syncHostParametersFromChain();
    };

    auto scenes = std::make_shared<ScenesHandler> (sceneManager);
    scenes->onChanged = [this, touched]
    {
        touched();
        // A scene recall flips effects on and off; the host's automation lanes
        // have to hear about it or they would overwrite it on the next block.
        processor.syncHostParametersFromChain();
    };

    channelStore.onChanged = [this, touched]
    {
        touched();
        processor.syncHostParametersFromChain();
    };

    auto pins = std::make_shared<PinsHandler> (pinnedControls, registry);
    pins->onChanged = touched;

    dispatcher.registerHandler ("/api/effects", effects);
    dispatcher.registerHandler ("/api/parameters",
                                std::make_shared<ParametersHandler> (registry, "master", "volumeDb"));
    dispatcher.registerHandler ("/api/presets", presets);
    dispatcher.registerHandler ("/api/scenes", scenes);
    dispatcher.registerHandler ("/api/pins", pins);
    dispatcher.registerHandler ("/api/ir", std::make_shared<IrHandler> (processor.getIrLibrary()));
    dispatcher.registerHandler ("/api/nam", std::make_shared<NamHandler> (processor.getNamLibrary()));
    dispatcher.registerHandler ("/api/levels", levelsHandler);
    dispatcher.registerHandler ("/api/health", std::make_shared<HealthHandler>());
}

HttpHandler::Response PluginBackend::call (const juce::String& method,
                                           const juce::String& path,
                                           const juce::String& query,
                                           const juce::String& body)
{
    // Anything that reads a file -- an impulse response, an amp model, a preset
    // -- happens right here, so it must be the message thread. The WebView
    // delivers native function calls there, which is the whole reason this is
    // safe without a lock.
    JUCE_ASSERT_MESSAGE_THREAD

    return dispatcher.dispatch (method.toStdString(),
                                path.toStdString(),
                                query.toStdString(),
                                body.toStdString());
}

juce::var PluginBackend::captureLayout() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty (kScenesProperty, sceneManager.toVar());
    root->setProperty (kChannelsProperty, channelStore.toVar());
    root->setProperty (kPinsProperty, pinnedControls.toVar());

    return juce::var (root);
}

void PluginBackend::applyLayout (const juce::var& layout)
{
    if (! layout.isObject())
    {
        // Nothing stored: seed the channels from the sound that is loaded rather
        // than leaving all four holding a default nobody chose.
        channelStore.resetToCurrent();
        sceneManager.resetToCurrent();
        return;
    }

    if (const auto scenes = layout[kScenesProperty]; scenes.isArray())
        sceneManager.fromVar (scenes);
    else
        sceneManager.resetToCurrent();

    if (const auto channels = layout[kChannelsProperty]; channels.isObject())
        channelStore.fromVar (channels);
    else
        channelStore.resetToCurrent();

    pinnedControls.fromVar (layout[kPinsProperty], &processor.getRegistry());
}
} // namespace milodikfx::plugin
