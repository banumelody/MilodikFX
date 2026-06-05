#pragma once

#include <JuceHeader.h>

namespace milodikfx::preset
{
struct PresetState final
{
    int schemaVersion = 1;

    bool globalBypass = false;

    bool cleanBoostEnabled = true;
    float cleanBoostGainDb = 0.0f;

    bool overdriveEnabled = true;
    float overdriveDrivePct = 0.0f;
    float overdriveLevelPct = 100.0f;

    bool eqEnabled = true;
    float eqBassDb = 0.0f;
    float eqMidDb = 0.0f;
    float eqTrebleDb = 0.0f;
};

class PresetManager final
{
public:
    explicit PresetManager (juce::File presetsDirectoryIn);

    juce::File getPresetsDirectory() const;

    juce::StringArray listPresets() const;

    bool savePreset (const juce::String& presetName, const PresetState& state) const;
    bool loadPreset (const juce::String& presetName, PresetState& outState) const;
    bool deletePreset (const juce::String& presetName) const;

    bool ensurePresetExists (const juce::String& presetName, const PresetState& defaultState) const;

    static juce::String sanitisePresetName (const juce::String& name);

private:
    juce::File directory;

    juce::File getPresetFile (const juce::String& presetName) const;

    static juce::var stateToVar (const juce::String& presetName, const PresetState& state);
    static bool varToState (const juce::var& v, PresetState& outState);

    static bool getBool (const juce::NamedValueSet& props, const juce::Identifier& key, bool defaultValue);
    static double getNumber (const juce::NamedValueSet& props, const juce::Identifier& key, double defaultValue);
};
} // namespace milodikfx::preset
