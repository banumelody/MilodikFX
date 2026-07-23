#pragma once

#include <JuceHeader.h>

#include <array>
#include <map>
#include <string>

#include "api/ParameterRegistry.h"
#include "preset/ChannelStore.h"

namespace milodikfx::preset
{
/**
 * Four on/off snapshots you can jump between while playing.
 *
 * Deliberately no raw parameter values -- only the enable flags and, when a
 * channel store is attached, which channel each effect should be on. A scene
 * change mid-song has to be instant and predictable: every processor already
 * has a tested enable/disable path, and a channel is a *named state you built
 * yourself*, shown as a tab on the card. So a scene jumps only to things you can
 * see -- never to a hidden value on a control you were not touching. A full
 * parameter snapshot is what presets are for.
 *
 * Scenes live inside the preset, so one preset carries its own four.
 */
class SceneManager final
{
public:
    static constexpr int kNumScenes = 4;

    struct Scene
    {
        juce::String name;

        /** effectId -> enabled. An effect absent here is left alone on recall. */
        std::map<std::string, bool> enabled;

        /** effectId -> channel index (0-3). Empty when no channel store is set. */
        std::map<std::string, int> channels;

        /** False until something has been stored, so an untouched slot recalls nothing. */
        bool populated = false;
    };

    explicit SceneManager (milodikfx::api::ParameterRegistry& registryToUse);

    /** When set, scenes also capture and recall each effect's channel. Optional:
        a plugin build has no channel store. */
    void setChannelStore (milodikfx::preset::ChannelStore* store) { channelStore = store; }

    /** Stores the chain's current on/off pattern into a slot. */
    bool capture (int index);

    /** Applies a slot. Returns false for an empty slot or a bad index. */
    bool recall (int index);

    /** Sets one effect's flag within a slot without disturbing the live chain. */
    bool setEffectEnabled (int index, const juce::String& effectId, bool enabled);

    bool setName (int index, const juce::String& name);

    int getActiveIndex() const noexcept { return activeIndex; }
    const Scene& getScene (int index) const;

    /** Serialised into the preset file. */
    juce::var toVar() const;
    void fromVar (const juce::var& value);

    /** Default names, every slot holding the chain as it is now. */
    void resetToCurrent();

    static bool isValidIndex (int index) noexcept { return index >= 0 && index < kNumScenes; }

private:
    milodikfx::api::ParameterRegistry& registry;
    milodikfx::preset::ChannelStore* channelStore = nullptr;

    std::array<Scene, kNumScenes> scenes;

    /** -1 when the chain has been changed since the last recall. */
    int activeIndex = -1;
};
} // namespace milodikfx::preset
