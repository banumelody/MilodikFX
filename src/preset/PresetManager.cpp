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
    // Carry forward whatever the file already said about itself. Overwriting a
    // preset to change its sound must not throw away its notes and tags.
    PresetDocument document;
    loadDocument (presetName, document);

    document.state = state;

    return saveDocument (presetName, document);
}

bool PresetManager::saveDocument (const juce::String& presetName, const PresetDocument& document) const
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

    root->setProperty ("description", document.metadata.description);
    root->setProperty ("favourite", document.metadata.favourite);
    root->setProperty ("notes", document.metadata.notes);

    juce::Array<juce::var> tags;

    for (const auto& tag : document.metadata.tags)
        if (tag.trim().isNotEmpty())
            tags.add (tag.trim());

    root->setProperty ("tags", tags);

    if (document.scenes.isArray())
        root->setProperty ("scenes", document.scenes);

    root->setProperty ("state", document.state);

    return file.replaceWithText (juce::JSON::toString (juce::var (root), false));
}

bool PresetManager::loadPreset (const juce::String& presetName, juce::var& outState) const
{
    PresetDocument document;

    if (! loadDocument (presetName, document))
        return false;

    outState = document.state;
    return true;
}

bool PresetManager::loadDocument (const juce::String& presetName, PresetDocument& outDocument) const
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

    outDocument.state = state;
    outDocument.scenes = parsed["scenes"];

    // Absent in a version 2 file, which is fine: the fields simply stay empty
    // rather than the whole preset being refused.
    outDocument.metadata.description = parsed["description"].toString();
    outDocument.metadata.notes = parsed["notes"].toString();
    outDocument.metadata.favourite = (bool) parsed["favourite"];
    outDocument.metadata.savedAt = parsed["savedAt"].toString();

    outDocument.metadata.tags.clear();

    if (const auto* tags = parsed["tags"].getArray())
        for (const auto& tag : *tags)
            if (const auto text = tag.toString().trim(); text.isNotEmpty())
                outDocument.metadata.tags.addIfNotAlreadyThere (text);

    return true;
}

bool PresetManager::updateMetadata (const juce::String& presetName, const PresetMetadata& metadata) const
{
    PresetDocument document;

    if (! loadDocument (presetName, document))
        return false;

    document.metadata = metadata;

    return saveDocument (presetName, document);
}

juce::String PresetManager::exportPreset (const juce::String& presetName) const
{
    const auto file = getPresetFile (presetName);

    if (file == juce::File() || ! file.existsAsFile())
        return {};

    return file.loadFileAsString();
}

juce::String PresetManager::importPreset (const juce::String& presetName, const juce::String& json) const
{
    juce::var parsed;

    if (! juce::JSON::parse (json, parsed).wasOk() || ! parsed.isObject())
        return {};

    // A file without a state is not a preset, whatever else it contains.
    // Accepting one would put an entry in the library that loads into nothing.
    if (! parsed["state"].isObject())
        return {};

    PresetDocument document;
    document.state = parsed["state"];
    document.scenes = parsed["scenes"];
    document.metadata.description = parsed["description"].toString();
    document.metadata.notes = parsed["notes"].toString();
    document.metadata.favourite = (bool) parsed["favourite"];

    if (const auto* tags = parsed["tags"].getArray())
        for (const auto& tag : *tags)
            if (const auto text = tag.toString().trim(); text.isNotEmpty())
                document.metadata.tags.addIfNotAlreadyThere (text);

    // The requested name wins, falling back to whatever the file called itself,
    // so importing without naming it still lands somewhere sensible.
    auto name = sanitisePresetName (presetName);

    if (name.isEmpty())
        name = sanitisePresetName (parsed["name"].toString());

    if (name.isEmpty())
        return {};

    return saveDocument (name, document) ? name : juce::String();
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
