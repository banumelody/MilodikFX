#pragma once

#include <JuceHeader.h>

#include <array>
#include <map>
#include <string>

#include "api/ParameterRegistry.h"

namespace milodikfx::preset
{
/**
 * Four on/off snapshots you can jump between while playing.
 *
 * Deliberately *only* the enable flags, not parameter values. A scene change
 * mid-song has to be instant and predictable: every processor already has a
 * tested enable/disable path, so switching cannot glitch, and the knobs stay
 * where you left them rather than jumping somewhere you cannot see. A full
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

        /** False until something has been stored, so an untouched slot recalls nothing. */
        bool populated = false;
    };

    explicit SceneManager (milodikfx::api::ParameterRegistry& registryToUse);

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

    std::array<Scene, kNumScenes> scenes;

    /** -1 when the chain has been changed since the last recall. */
    int activeIndex = -1;
};
} // namespace milodikfx::preset
