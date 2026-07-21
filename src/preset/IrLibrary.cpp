#include "preset/IrLibrary.h"

namespace milodikfx::preset
{
namespace
{
const char* categoryFolder (IrLibrary::Category category)
{
    return category == IrLibrary::Category::cabinet ? "Cabinets" : "Reverbs";
}

/** Extensions juce::AudioFormatManager::registerBasicFormats can open. */
const char* kSearchPattern = "*.wav;*.aiff;*.aif;*.flac;*.ogg";
} // namespace

IrLibrary::IrLibrary (juce::File rootDirectory)
    : root (std::move (rootDirectory))
{
    for (const auto category : { Category::cabinet, Category::reverb })
    {
        auto dir = getDirectory (category);

        if (! dir.exists())
            dir.createDirectory();
    }
}

juce::File IrLibrary::getDirectory (Category category) const
{
    return root.getChildFile (categoryFolder (category));
}

juce::String IrLibrary::sanitiseName (const juce::String& name)
{
    auto trimmed = name.trim();

    if (trimmed.isEmpty())
        return {};

    // Identical treatment to preset names: strip anything that could turn a
    // name into a path, then flatten dot runs so nothing reads as relative.
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

juce::StringArray IrLibrary::list (Category category) const
{
    juce::StringArray names;

    const auto dir = getDirectory (category);

    if (! dir.isDirectory())
        return names;

    juce::Array<juce::File> files;
    dir.findChildFiles (files, juce::File::findFiles, false, kSearchPattern);

    for (const auto& file : files)
        names.add (file.getFileNameWithoutExtension());

    names.sortNatural();
    return names;
}

juce::File IrLibrary::resolve (Category category, const juce::String& name) const
{
    const auto safe = sanitiseName (name);

    if (safe.isEmpty())
        return {};

    const auto dir = getDirectory (category);

    juce::Array<juce::File> files;
    dir.findChildFiles (files, juce::File::findFiles, false, kSearchPattern);

    for (const auto& file : files)
        if (file.getFileNameWithoutExtension().equalsIgnoreCase (safe))
            return file.isAChildOf (dir) ? file : juce::File();

    return {};
}

juce::String IrLibrary::import (Category category, const juce::String& name, const juce::MemoryBlock& data) const
{
    const auto safe = sanitiseName (name);

    if (safe.isEmpty() || data.getSize() == 0)
        return {};

    auto dir = getDirectory (category);

    if (! dir.exists() && ! dir.createDirectory())
        return {};

    // Everything is stored as .wav regardless of what the name claimed; the
    // format manager reads by content, and a fixed extension keeps resolve()
    // simple.
    auto target = dir.getChildFile (safe + ".wav");

    if (! target.isAChildOf (dir))
        return {};

    return target.replaceWithData (data.getData(), data.getSize()) ? safe : juce::String();
}
} // namespace milodikfx::preset
