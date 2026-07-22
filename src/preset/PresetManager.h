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
/**
 * How a preset is organised, as opposed to what it sounds like.
 *
 * Kept beside `state` rather than inside it: `state` is exactly what
 * ParameterRegistry::captureState produced, and mixing filing-cabinet fields
 * into it would make the DSP snapshot depend on how presets happen to be
 * catalogued today.
 */
struct PresetMetadata
{
    juce::String description;
    juce::StringArray tags;
    bool favourite = false;
    juce::String notes;

    /** Filled on load; ignored on save, which always stamps the current time. */
    juce::String savedAt;
};

/** Everything a preset file holds. */
struct PresetDocument
{
    juce::var state;
    PresetMetadata metadata;

    /** Scene slots, opaque here -- SceneManager owns the shape. */
    juce::var scenes;

    /** Per-effect A/B/C/D channels, opaque here -- ChannelStore owns the shape. */
    juce::var channels;
};

class PresetManager final
{
public:
    /** 3 added metadata and scenes; 4 added per-effect channels. Older files
        still load; the new fields simply come back empty. */
    static constexpr int kSchemaVersion = 4;

    explicit PresetManager (juce::File presetsDirectoryIn);

    juce::File getPresetsDirectory() const;

    juce::StringArray listPresets() const;

    /**
     * Saves the parameter snapshot, keeping any metadata and scenes the file
     * already had. Re-saving a preset must not quietly discard its notes.
     */
    bool savePreset (const juce::String& presetName, const juce::var& state) const;

    /** Saves everything, replacing whatever was there. */
    bool saveDocument (const juce::String& presetName, const PresetDocument& document) const;

    bool loadPreset (const juce::String& presetName, juce::var& outState) const;
    bool loadDocument (const juce::String& presetName, PresetDocument& outDocument) const;

    /** Rewrites the metadata alone, leaving the sound untouched. */
    bool updateMetadata (const juce::String& presetName, const PresetMetadata& metadata) const;

    bool deletePreset (const juce::String& presetName) const;

    /** The whole file as text, for export. Empty when there is no such preset. */
    juce::String exportPreset (const juce::String& presetName) const;

    /**
     * Writes an exported document back, under `presetName`.
     *
     * Rejects anything without a usable `state`, so a truncated or unrelated
     * JSON file cannot land in the library as a preset that loads into silence.
     * Returns the name it was stored under, or an empty string on failure.
     */
    juce::String importPreset (const juce::String& presetName, const juce::String& json) const;

    bool presetExists (const juce::String& presetName) const;
    bool ensurePresetExists (const juce::String& presetName, const juce::var& defaultState) const;

    /** Strips anything that cannot appear in a Windows filename. */
    static juce::String sanitisePresetName (const juce::String& name);

private:
    juce::File directory;

    juce::File getPresetFile (const juce::String& presetName) const;
};
} // namespace milodikfx::preset
