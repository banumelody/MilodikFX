#pragma once

#include <JuceHeader.h>

namespace milodikfx::preset
{
/**
 * The NAM amp-head models available on disk.
 *
 * Same filename discipline as IrLibrary and PresetManager: a name arriving over
 * HTTP is sanitised before it touches the filesystem, and the resolved file must
 * still sit inside the models directory.
 */
class NamLibrary final
{
public:
    explicit NamLibrary (juce::File rootDirectory);

    juce::File getDirectory() const { return root; }

    /** Names (without extension) of every .nam file present. */
    juce::StringArray list() const;

    /**
     * Resolves a name to a file.
     * @return a non-existent File when the name is empty, unsafe, or unknown.
     */
    juce::File resolve (const juce::String& name) const;

    /** Writes a model into the library. Returns the stored name, or empty. */
    juce::String import (const juce::String& name, const juce::MemoryBlock& data) const;

    static juce::String sanitiseName (const juce::String& name);

private:
    juce::File root;
};
} // namespace milodikfx::preset
