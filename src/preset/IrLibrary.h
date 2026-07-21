#pragma once

#include <JuceHeader.h>

namespace milodikfx::preset
{
/**
 * The impulse responses available on disk, in two categories.
 *
 * Same shape and the same filename discipline as PresetManager: a name that
 * arrives over HTTP is sanitised before it ever reaches the filesystem, and the
 * resolved file must still sit inside its own directory.
 */
class IrLibrary final
{
public:
    enum class Category
    {
        cabinet,
        reverb
    };

    explicit IrLibrary (juce::File rootDirectory);

    juce::File getDirectory (Category category) const;

    /** Names (without extension) of every readable impulse response. */
    juce::StringArray list (Category category) const;

    /**
     * Resolves a name to a file.
     * @return a non-existent File when the name is empty, unsafe, or unknown.
     */
    juce::File resolve (Category category, const juce::String& name) const;

    /** Writes an impulse response into the library. Returns the stored name. */
    juce::String import (Category category, const juce::String& name, const juce::MemoryBlock& data) const;

    static juce::String sanitiseName (const juce::String& name);

private:
    juce::File root;
};
} // namespace milodikfx::preset
