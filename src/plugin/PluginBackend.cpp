#include "plugin/PluginBackend.h"

#include "api/EffectsHandler.h"
#include "api/HealthHandler.h"
#include "api/IrHandler.h"
#include "api/NamHandler.h"
#include "api/ParametersHandler.h"
#include "api/PinsHandler.h"
#include "api/PresetsHandler.h"
#include "api/ModulationHandler.h"
#include "api/ScenesHandler.h"
#include "api/TunerHandler.h"
#include "plugin/PluginProcessor.h"

namespace milodikfx::plugin
{
namespace
{
const juce::Identifier kScenesProperty { "scenes" };
const juce::Identifier kChannelsProperty { "channels" };
const juce::Identifier kPinsProperty { "pins" };
const juce::Identifier kModifiersProperty { "modifiers" };
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

    // A wah is the contour frequency swept by a pedal; the pedal reaches the
    // engine as controller messages on the plugin's MIDI bus.
    auto modifiers = std::make_shared<ModulationHandler> (registry, processor.getModulation());
    modifiers->onChanged = touched;

    dispatcher.registerHandler ("/api/effects", effects);
    dispatcher.registerHandler ("/api/parameters",
                                std::make_shared<ParametersHandler> (registry, "master", "volumeDb"));
    dispatcher.registerHandler ("/api/presets", presets);
    dispatcher.registerHandler ("/api/scenes", scenes);
    dispatcher.registerHandler ("/api/pins", pins);
    dispatcher.registerHandler ("/api/modifiers", modifiers);
    dispatcher.registerHandler ("/api/ir", std::make_shared<IrHandler> (processor.getIrLibrary()));
    dispatcher.registerHandler ("/api/nam", std::make_shared<NamHandler> (processor.getNamLibrary()));
    dispatcher.registerHandler ("/api/levels", levelsHandler);

    // The processor taps the input before the chain for this; without the
    // endpoint the tap was plumbed in and the panel had nothing to talk to.
    // Analysis stays off until the panel switches it on, so an unopened tuner
    // costs one atomic read per block.
    dispatcher.registerHandler ("/api/tuner", std::make_shared<TunerHandler> (processor.getTuner()));

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
    root->setProperty (kModifiersProperty, captureModifiers());

    return juce::var (root);
}

juce::var PluginBackend::captureModifiers() const
{
    juce::Array<juce::var> array;
    auto& modulation = processor.getModulation();

    for (int slot = 0; slot < milodikfx::dsp::ModulationEngine::kMaxModifiers; ++slot)
    {
        const auto info = modulation.getModifier (slot);

        if (! info.active)
            continue;

        auto* entry = new juce::DynamicObject();
        entry->setProperty ("slot", slot);
        entry->setProperty ("effect", juce::String (info.effectId));
        entry->setProperty ("parameter", juce::String (info.parameterId));
        entry->setProperty ("source", info.source);
        entry->setProperty ("low", info.low);
        entry->setProperty ("high", info.high);
        entry->setProperty ("rateHz", info.rateHz);
        entry->setProperty ("expressionCc", info.expressionCc);
        entry->setProperty ("syncDivision", info.syncDivision);

        // The offset, not the swept sample: a project saved mid-sweep must
        // reopen on the centre the knob shows, not wherever the LFO happened
        // to be at the moment the host asked.
        entry->setProperty ("baseOffset", info.baseOffset);

        array.add (juce::var (entry));
    }

    return array;
}

void PluginBackend::applyModifiers (const juce::var& value)
{
    auto& modulation = processor.getModulation();
    auto& registry = processor.getRegistry();

    for (int slot = 0; slot < milodikfx::dsp::ModulationEngine::kMaxModifiers; ++slot)
        modulation.clearModifier (slot);

    const auto* array = value.getArray();

    if (array == nullptr)
        return;

    for (const auto& item : *array)
    {
        const auto slot = (int) item["slot"];
        const auto effectId = item["effect"].toString().toStdString();
        const auto parameterId = item["parameter"].toString().toStdString();

        if (! milodikfx::dsp::ModulationEngine::isValidSlot (slot))
            continue;

        // Resolved against this build's registry: a modifier naming something
        // that no longer exists must not become a slot sweeping nothing.
        const auto* target = registry.findParameter (effectId, parameterId);

        if (target == nullptr)
            continue;

        milodikfx::dsp::ModulationEngine::Config config;
        config.source = (milodikfx::dsp::ModulationEngine::Source) (int) item["source"];
        config.low = (float) (double) item["low"];
        config.high = (float) (double) item["high"];
        config.rateHz = (float) (double) item["rateHz"];
        config.expressionCc = item.hasProperty ("expressionCc") ? (int) item["expressionCc"] : -1;
        config.syncDivision = (int) item["syncDivision"];
        config.baseOffset = (float) (double) item["baseOffset"];

        modulation.setModifier (slot, target, effectId, parameterId, config);
    }
}

void PluginBackend::applyLayout (const juce::var& layout)
{
    if (! layout.isObject())
    {
        // Nothing stored: seed the channels from the sound that is loaded rather
        // than leaving all four holding a default nobody chose.
        channelStore.resetToCurrent();
        sceneManager.resetToCurrent();
        applyModifiers (juce::var());
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
    applyModifiers (layout[kModifiersProperty]);
}
} // namespace milodikfx::plugin
