#include "preset/NamLibrary.h"

namespace milodikfx::preset
{
namespace
{
constexpr const char* kSearchPattern = "*.nam";
} // namespace

NamLibrary::NamLibrary (juce::File rootDirectory)
    : root (std::move (rootDirectory))
{
    if (! root.exists())
        root.createDirectory();
}

juce::String NamLibrary::sanitiseName (const juce::String& name)
{
    auto trimmed = name.trim();

    if (trimmed.isEmpty())
        return {};

    // Identical treatment to impulse-response and preset names: strip anything
    // that could turn a name into a path, then flatten dot runs so nothing reads
    // as relative.
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

juce::StringArray NamLibrary::list() const
{
    juce::StringArray names;

    if (! root.isDirectory())
        return names;

    juce::Array<juce::File> files;
    root.findChildFiles (files, juce::File::findFiles, false, kSearchPattern);

    for (const auto& file : files)
        names.add (file.getFileNameWithoutExtension());

    names.sortNatural();
    return names;
}

juce::File NamLibrary::resolve (const juce::String& name) const
{
    const auto safe = sanitiseName (name);

    if (safe.isEmpty())
        return {};

    juce::Array<juce::File> files;
    root.findChildFiles (files, juce::File::findFiles, false, kSearchPattern);

    for (const auto& file : files)
        if (file.getFileNameWithoutExtension().equalsIgnoreCase (safe))
            return file.isAChildOf (root) ? file : juce::File();

    return {};
}

juce::String NamLibrary::import (const juce::String& name, const juce::MemoryBlock& data) const
{
    const auto safe = sanitiseName (name);

    if (safe.isEmpty() || data.getSize() == 0)
        return {};

    if (! root.exists() && ! root.createDirectory())
        return {};

    auto target = root.getChildFile (safe + ".nam");

    if (! target.isAChildOf (root))
        return {};

    return target.replaceWithData (data.getData(), data.getSize()) ? safe : juce::String();
}
} // namespace milodikfx::preset
