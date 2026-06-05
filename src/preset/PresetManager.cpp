#include "preset/PresetManager.h"

namespace milodikfx::preset
{
namespace
{
constexpr int kSchemaVersion = 1;

static float clampDb12 (float db) noexcept
{
    return juce::jlimit (-12.0f, 12.0f, db);
}

static float clampPct (float pct) noexcept
{
    return juce::jlimit (0.0f, 100.0f, pct);
}

static juce::StringArray findPresetFiles (const juce::File& dir)
{
    juce::StringArray names;

    if (! dir.exists())
        return names;

    juce::Array<juce::File> files;
    dir.findChildFiles (files, juce::File::findFiles, false, "*.json");

    for (const auto& f : files)
        names.add (f.getFileNameWithoutExtension());

    names.sort (true);
    return names;
}
} // namespace

PresetManager::PresetManager (juce::File presetsDirectoryIn)
    : directory (std::move (presetsDirectoryIn))
{
}

juce::File PresetManager::getPresetsDirectory() const
{
    return directory;
}

juce::String PresetManager::sanitisePresetName (const juce::String& name)
{
    auto s = name.trim();

    if (s.isEmpty())
        return {};

    juce::String out;
    out.preallocateBytes (s.getNumBytesAsUTF8());

    for (const auto c : s)
    {
        if (juce::CharacterFunctions::isLetterOrDigit (c)
            || c == ' ' || c == '-' || c == '_' || c == '.')
        {
            out += c;
        }
        else
        {
            out += '_';
        }
    }

    out = out.trim();

    while (out.contains ("  "))
        out = out.replace ("  ", " ");

    return out;
}

juce::File PresetManager::getPresetFile (const juce::String& presetName) const
{
    const auto safe = sanitisePresetName (presetName);
    if (safe.isEmpty())
        return {};

    return directory.getChildFile (safe + ".json");
}

juce::var PresetManager::stateToVar (const juce::String& presetName, const PresetState& state)
{
    auto root = juce::DynamicObject::Ptr (new juce::DynamicObject());
    root->setProperty ("schemaVersion", kSchemaVersion);
    root->setProperty ("name", presetName);

    auto dsp = juce::DynamicObject::Ptr (new juce::DynamicObject());

    dsp->setProperty ("globalBypass", state.globalBypass);

    auto clean = juce::DynamicObject::Ptr (new juce::DynamicObject());
    clean->setProperty ("enabled", state.cleanBoostEnabled);
    clean->setProperty ("gainDb", (double) state.cleanBoostGainDb);
    dsp->setProperty ("cleanBoost", juce::var (clean.get()));

    auto od = juce::DynamicObject::Ptr (new juce::DynamicObject());
    od->setProperty ("enabled", state.overdriveEnabled);
    od->setProperty ("drivePct", (double) state.overdriveDrivePct);
    od->setProperty ("levelPct", (double) state.overdriveLevelPct);
    dsp->setProperty ("overdrive", juce::var (od.get()));

    auto eq = juce::DynamicObject::Ptr (new juce::DynamicObject());
    eq->setProperty ("enabled", state.eqEnabled);
    eq->setProperty ("bassDb", (double) state.eqBassDb);
    eq->setProperty ("midDb", (double) state.eqMidDb);
    eq->setProperty ("trebleDb", (double) state.eqTrebleDb);
    dsp->setProperty ("eq", juce::var (eq.get()));

    root->setProperty ("dsp", juce::var (dsp.get()));

    return juce::var (root.get());
}

bool PresetManager::getBool (const juce::NamedValueSet& props, const juce::Identifier& key, bool defaultValue)
{
    if (props.contains (key))
        return (bool) props[key];

    return defaultValue;
}

double PresetManager::getNumber (const juce::NamedValueSet& props, const juce::Identifier& key, double defaultValue)
{
    if (props.contains (key))
        return (double) props[key];

    return defaultValue;
}

bool PresetManager::varToState (const juce::var& v, PresetState& outState)
{
    auto* rootObj = v.getDynamicObject();
    if (rootObj == nullptr)
        return false;

    const auto& rootProps = rootObj->getProperties();

    outState.schemaVersion = (int) getNumber (rootProps, "schemaVersion", kSchemaVersion);

    const auto dspVar = rootProps["dsp"];
    auto* dspObj = dspVar.getDynamicObject();
    if (dspObj == nullptr)
        return false;

    const auto& dspProps = dspObj->getProperties();

    outState.globalBypass = getBool (dspProps, "globalBypass", false);

    {
        const auto cleanVar = dspProps["cleanBoost"];
        if (auto* cleanObj = cleanVar.getDynamicObject())
        {
            const auto& p = cleanObj->getProperties();
            outState.cleanBoostEnabled = getBool (p, "enabled", true);
            outState.cleanBoostGainDb = (float) getNumber (p, "gainDb", 0.0);
        }
    }

    {
        const auto odVar = dspProps["overdrive"];
        if (auto* odObj = odVar.getDynamicObject())
        {
            const auto& p = odObj->getProperties();
            outState.overdriveEnabled = getBool (p, "enabled", true);
            outState.overdriveDrivePct = (float) getNumber (p, "drivePct", 0.0);
            outState.overdriveLevelPct = (float) getNumber (p, "levelPct", 100.0);
        }
    }

    {
        const auto eqVar = dspProps["eq"];
        if (auto* eqObj = eqVar.getDynamicObject())
        {
            const auto& p = eqObj->getProperties();
            outState.eqEnabled = getBool (p, "enabled", true);
            outState.eqBassDb = (float) getNumber (p, "bassDb", 0.0);
            outState.eqMidDb = (float) getNumber (p, "midDb", 0.0);
            outState.eqTrebleDb = (float) getNumber (p, "trebleDb", 0.0);
        }
    }

    outState.cleanBoostGainDb = juce::jlimit (0.0f, 24.0f, outState.cleanBoostGainDb);
    outState.overdriveDrivePct = clampPct (outState.overdriveDrivePct);
    outState.overdriveLevelPct = clampPct (outState.overdriveLevelPct);

    outState.eqBassDb = clampDb12 (outState.eqBassDb);
    outState.eqMidDb = clampDb12 (outState.eqMidDb);
    outState.eqTrebleDb = clampDb12 (outState.eqTrebleDb);

    return true;
}

juce::StringArray PresetManager::listPresets() const
{
    directory.createDirectory();
    return findPresetFiles (directory);
}

bool PresetManager::savePreset (const juce::String& presetName, const PresetState& state) const
{
    const auto safe = sanitisePresetName (presetName);
    if (safe.isEmpty())
        return false;

    directory.createDirectory();

    const auto file = getPresetFile (safe);
    if (file == juce::File{})
        return false;

    const auto v = stateToVar (safe, state);
    const auto json = juce::JSON::toString (v, true);

    return file.replaceWithText (json);
}

bool PresetManager::loadPreset (const juce::String& presetName, PresetState& outState) const
{
    const auto safe = sanitisePresetName (presetName);
    if (safe.isEmpty())
        return false;

    const auto file = getPresetFile (safe);
    if (! file.existsAsFile())
        return false;

    const auto text = file.loadFileAsString();
    if (text.isEmpty())
        return false;

    const auto v = juce::JSON::parse (text);
    if (v.isVoid() || v.isUndefined())
        return false;

    return varToState (v, outState);
}

bool PresetManager::deletePreset (const juce::String& presetName) const
{
    const auto safe = sanitisePresetName (presetName);
    if (safe.isEmpty())
        return false;

    const auto file = getPresetFile (safe);
    if (! file.existsAsFile())
        return false;

    return file.deleteFile();
}

bool PresetManager::ensurePresetExists (const juce::String& presetName, const PresetState& defaultState) const
{
    const auto safe = sanitisePresetName (presetName);
    if (safe.isEmpty())
        return false;

    directory.createDirectory();

    const auto file = getPresetFile (safe);
    if (file.existsAsFile())
        return true;

    return savePreset (safe, defaultState);
}
} // namespace milodikfx::preset
