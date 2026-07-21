#include "preset/PresetManager.h"

namespace milodikfx::preset
{
namespace
{
constexpr const char* kFileExtension = ".json";
} // namespace

PresetManager::PresetManager (juce::File presetsDirectoryIn)
    : directory (std::move (presetsDirectoryIn))
{
    if (! directory.exists())
        directory.createDirectory();
}

juce::File PresetManager::getPresetsDirectory() const
{
    return directory;
}

juce::String PresetManager::sanitisePresetName (const juce::String& name)
{
    auto trimmed = name.trim();

    if (trimmed.isEmpty())
        return {};

    // createLegalFileName strips path separators, but "../x" collapses to "..x",
    // so also flatten any remaining dot runs and leading dots. The result is
    // always a plain filename with no relative-path meaning.
    auto safe = juce::File::createLegalFileName (trimmed);
    safe = safe.replaceCharacters ("\\/", "__");

    while (safe.contains (".."))
        safe = safe.replace ("..", ".");

    while (safe.startsWithChar ('.') || safe.startsWithChar (' '))
        safe = safe.substring (1);

    while (safe.endsWithChar ('.') || safe.endsWithChar (' '))
        safe = safe.dropLastCharacters (1);

    return safe.trim();
}

juce::File PresetManager::getPresetFile (const juce::String& presetName) const
{
    const auto safe = sanitisePresetName (presetName);

    if (safe.isEmpty())
        return {};

    return directory.getChildFile (safe + kFileExtension);
}

juce::StringArray PresetManager::listPresets() const
{
    juce::StringArray names;

    if (! directory.isDirectory())
        return names;

    juce::Array<juce::File> files;
    directory.findChildFiles (files, juce::File::findFiles, false, juce::String ("*") + kFileExtension);

    for (const auto& file : files)
        names.add (file.getFileNameWithoutExtension());

    names.sortNatural();
    return names;
}

bool PresetManager::savePreset (const juce::String& presetName, const juce::var& state) const
{
    const auto file = getPresetFile (presetName);

    if (file == juce::File())
        return false;

    if (! directory.exists() && ! directory.createDirectory())
        return false;

    auto* root = new juce::DynamicObject();
    root->setProperty ("schemaVersion", kSchemaVersion);
    root->setProperty ("name", sanitisePresetName (presetName));
    root->setProperty ("savedAt", juce::Time::getCurrentTime().toISO8601 (true));
    root->setProperty ("state", state);

    return file.replaceWithText (juce::JSON::toString (juce::var (root), false));
}

bool PresetManager::loadPreset (const juce::String& presetName, juce::var& outState) const
{
    const auto file = getPresetFile (presetName);

    if (file == juce::File() || ! file.existsAsFile())
        return false;

    juce::var parsed;

    if (! juce::JSON::parse (file.loadFileAsString(), parsed).wasOk() || ! parsed.isObject())
        return false;

    const auto state = parsed["state"];

    if (! state.isObject())
        return false;

    outState = state;
    return true;
}

bool PresetManager::deletePreset (const juce::String& presetName) const
{
    const auto file = getPresetFile (presetName);

    if (file == juce::File() || ! file.existsAsFile())
        return false;

    return file.deleteFile();
}

bool PresetManager::presetExists (const juce::String& presetName) const
{
    const auto file = getPresetFile (presetName);
    return file != juce::File() && file.existsAsFile();
}

bool PresetManager::ensurePresetExists (const juce::String& presetName, const juce::var& defaultState) const
{
    if (presetExists (presetName))
        return true;

    return savePreset (presetName, defaultState);
}
} // namespace milodikfx::preset
