#pragma once

#include <JuceHeader.h>

namespace milodikfx::preset
{
/**
 * Reads and writes preset files as JSON documents under a directory.
 *
 * The payload is an opaque juce::var produced by ParameterRegistry::captureState,
 * so adding a DSP parameter does not require touching this class at all.
 */
class PresetManager final
{
public:
    static constexpr int kSchemaVersion = 2;

    explicit PresetManager (juce::File presetsDirectoryIn);

    juce::File getPresetsDirectory() const;

    juce::StringArray listPresets() const;

    /** @param state a captured parameter snapshot; stored under "state". */
    bool savePreset (const juce::String& presetName, const juce::var& state) const;
    bool loadPreset (const juce::String& presetName, juce::var& outState) const;
    bool deletePreset (const juce::String& presetName) const;

    bool presetExists (const juce::String& presetName) const;
    bool ensurePresetExists (const juce::String& presetName, const juce::var& defaultState) const;

    /** Strips anything that cannot appear in a Windows filename. */
    static juce::String sanitisePresetName (const juce::String& name);

private:
    juce::File directory;

    juce::File getPresetFile (const juce::String& presetName) const;
};
} // namespace milodikfx::preset
