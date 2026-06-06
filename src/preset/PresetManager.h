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

    bool compressorEnabled = true;
    float compressorInputGainDb = 0.0f;
    float compressorThresholdDb = -24.0f;
    float compressorRatio = 4.0f;
    float compressorAttackMs = 10.0f;
    float compressorReleaseMs = 100.0f;

    bool reverbEnabled = true;
    float reverbRoomSize = 0.5f;
    float reverbDryWetMix = 0.5f;
    float reverbDecayTime = 2.0f;
    float reverbWidth = 1.0f;

    bool toneStackEnabled = true;
    float toneStackBassDb = 0.0f;
    float toneStackMidDb = 0.0f;
    float toneStackTrebleDb = 0.0f;
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
