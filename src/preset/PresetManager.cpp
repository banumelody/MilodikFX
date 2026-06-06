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

    // Metadata
    auto metadata = juce::DynamicObject::Ptr (new juce::DynamicObject());
    metadata->setProperty ("author", juce::var (juce::String (state.author)));
    metadata->setProperty ("description", juce::var (juce::String (state.description)));
    metadata->setProperty ("category", juce::var (juce::String (state.category)));
    metadata->setProperty ("createdAt", state.createdAt.toISO8601 (true));
    metadata->setProperty ("modifiedAt", state.modifiedAt.toISO8601 (true));
    
    auto tagsArray = juce::Array<juce::var>();
    for (int i = 0; i < state.tags.size(); ++i)
        tagsArray.add (juce::var (state.tags[i]));
    metadata->setProperty ("tags", tagsArray);
    
    root->setProperty ("metadata", juce::var (metadata.get()));

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

    auto compressor = juce::DynamicObject::Ptr (new juce::DynamicObject());
    compressor->setProperty ("enabled", state.compressorEnabled);
    compressor->setProperty ("inputGainDb", (double) state.compressorInputGainDb);
    compressor->setProperty ("thresholdDb", (double) state.compressorThresholdDb);
    compressor->setProperty ("ratio", (double) state.compressorRatio);
    compressor->setProperty ("attackMs", (double) state.compressorAttackMs);
    compressor->setProperty ("releaseMs", (double) state.compressorReleaseMs);
    dsp->setProperty ("compressor", juce::var (compressor.get()));

    auto reverb = juce::DynamicObject::Ptr (new juce::DynamicObject());
    reverb->setProperty ("enabled", state.reverbEnabled);
    reverb->setProperty ("roomSize", (double) state.reverbRoomSize);
    reverb->setProperty ("dryWetMix", (double) state.reverbDryWetMix);
    reverb->setProperty ("decayTime", (double) state.reverbDecayTime);
    reverb->setProperty ("width", (double) state.reverbWidth);
    dsp->setProperty ("reverb", juce::var (reverb.get()));

    auto toneStack = juce::DynamicObject::Ptr (new juce::DynamicObject());
    toneStack->setProperty ("enabled", state.toneStackEnabled);
    toneStack->setProperty ("bassDb", (double) state.toneStackBassDb);
    toneStack->setProperty ("midDb", (double) state.toneStackMidDb);
    toneStack->setProperty ("trebleDb", (double) state.toneStackTrebleDb);
    dsp->setProperty ("toneStack", juce::var (toneStack.get()));

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

    // Deserialize metadata
    {
        const auto metadataVar = rootProps["metadata"];
        if (auto* metadataObj = metadataVar.getDynamicObject())
        {
            const auto& metaProps = metadataObj->getProperties();
            
            if (metaProps.contains ("author"))
                outState.author = metaProps["author"].toString().toStdString();
            
            if (metaProps.contains ("description"))
                outState.description = metaProps["description"].toString().toStdString();
            
            if (metaProps.contains ("category"))
                outState.category = metaProps["category"].toString().toStdString();
            
            // Parse createdAt from ISO 8601 string
            if (metaProps.contains ("createdAt"))
            {
                const auto createdAtStr = metaProps["createdAt"].toString();
                if (! createdAtStr.isEmpty())
                {
                    outState.createdAt = juce::Time::fromISO8601 (createdAtStr);
                }
                else
                {
                    outState.createdAt = juce::Time::getCurrentTime();
                }
            }
            else
            {
                outState.createdAt = juce::Time::getCurrentTime();
            }
            
            // Parse modifiedAt from ISO 8601 string
            if (metaProps.contains ("modifiedAt"))
            {
                const auto modifiedAtStr = metaProps["modifiedAt"].toString();
                if (! modifiedAtStr.isEmpty())
                {
                    outState.modifiedAt = juce::Time::fromISO8601 (modifiedAtStr);
                }
                else
                {
                    outState.modifiedAt = juce::Time::getCurrentTime();
                }
            }
            else
            {
                outState.modifiedAt = juce::Time::getCurrentTime();
            }
            
            // Parse tags array
            if (metaProps.contains ("tags"))
            {
                const auto tagsVar = metaProps["tags"];
                if (tagsVar.isArray())
                {
                    for (int i = 0; i < tagsVar.size(); ++i)
                    {
                        outState.tags.add (tagsVar[i].toString());
                    }
                }
            }
        }
        else
        {
            // Set defaults if no metadata found
            outState.createdAt = juce::Time::getCurrentTime();
            outState.modifiedAt = juce::Time::getCurrentTime();
        }
    }

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

    {
        const auto compVar = dspProps["compressor"];
        if (auto* compObj = compVar.getDynamicObject())
        {
            const auto& p = compObj->getProperties();
            outState.compressorEnabled = getBool (p, "enabled", true);
            outState.compressorInputGainDb = (float) getNumber (p, "inputGainDb", 0.0);
            outState.compressorThresholdDb = (float) getNumber (p, "thresholdDb", -24.0);
            outState.compressorRatio = (float) getNumber (p, "ratio", 4.0);
            outState.compressorAttackMs = (float) getNumber (p, "attackMs", 10.0);
            outState.compressorReleaseMs = (float) getNumber (p, "releaseMs", 100.0);
        }
    }

    {
        const auto revVar = dspProps["reverb"];
        if (auto* revObj = revVar.getDynamicObject())
        {
            const auto& p = revObj->getProperties();
            outState.reverbEnabled = getBool (p, "enabled", true);
            outState.reverbRoomSize = (float) getNumber (p, "roomSize", 0.5);
            outState.reverbDryWetMix = (float) getNumber (p, "dryWetMix", 0.5);
            outState.reverbDecayTime = (float) getNumber (p, "decayTime", 2.0);
            outState.reverbWidth = (float) getNumber (p, "width", 1.0);
        }
    }

    {
        const auto tsVar = dspProps["toneStack"];
        if (auto* tsObj = tsVar.getDynamicObject())
        {
            const auto& p = tsObj->getProperties();
            outState.toneStackEnabled = getBool (p, "enabled", true);
            outState.toneStackBassDb = (float) getNumber (p, "bassDb", 0.0);
            outState.toneStackMidDb = (float) getNumber (p, "midDb", 0.0);
            outState.toneStackTrebleDb = (float) getNumber (p, "trebleDb", 0.0);
        }
    }

    outState.cleanBoostGainDb = juce::jlimit (0.0f, 24.0f, outState.cleanBoostGainDb);
    outState.overdriveDrivePct = clampPct (outState.overdriveDrivePct);
    outState.overdriveLevelPct = clampPct (outState.overdriveLevelPct);

    outState.eqBassDb = clampDb12 (outState.eqBassDb);
    outState.eqMidDb = clampDb12 (outState.eqMidDb);
    outState.eqTrebleDb = clampDb12 (outState.eqTrebleDb);

    outState.compressorInputGainDb = juce::jlimit (-12.0f, 12.0f, outState.compressorInputGainDb);
    outState.compressorThresholdDb = juce::jlimit (-60.0f, 0.0f, outState.compressorThresholdDb);
    outState.compressorRatio = juce::jlimit (1.0f, 16.0f, outState.compressorRatio);
    outState.compressorAttackMs = juce::jlimit (0.1f, 100.0f, outState.compressorAttackMs);
    outState.compressorReleaseMs = juce::jlimit (10.0f, 1000.0f, outState.compressorReleaseMs);

    outState.reverbRoomSize = juce::jlimit (0.0f, 1.0f, outState.reverbRoomSize);
    outState.reverbDryWetMix = juce::jlimit (0.0f, 1.0f, outState.reverbDryWetMix);
    outState.reverbDecayTime = juce::jlimit (0.5f, 10.0f, outState.reverbDecayTime);
    outState.reverbWidth = juce::jlimit (0.0f, 1.0f, outState.reverbWidth);

    outState.toneStackBassDb = clampDb12 (outState.toneStackBassDb);
    outState.toneStackMidDb = clampDb12 (outState.toneStackMidDb);
    outState.toneStackTrebleDb = clampDb12 (outState.toneStackTrebleDb);

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
