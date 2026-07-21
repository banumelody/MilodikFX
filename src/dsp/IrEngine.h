#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <memory>

namespace milodikfx::dsp
{
/**
 * Convolution front-end shared by the cabinet and the reverb.
 *
 * Wraps juce::dsp::Convolution, whose loadImpulseResponse is documented
 * wait-free and therefore safe to call while audio is running. prepare()
 * allocates, so it belongs in prepareToPlay only.
 *
 * The engine reports whether it actually has an impulse response loaded, so a
 * caller can fall back to its own algorithm rather than emitting silence when a
 * file is missing or unreadable.
 */
class IrEngine
{
public:
    IrEngine();
    ~IrEngine();

    void prepare (double sampleRate, int maximumBlockSize, int numChannels);
    void reset();

    /**
     * Loads an impulse response from disk.
     * @return false if the file does not exist or is not a readable audio file.
     */
    bool loadFromFile (const juce::File& file);

    /** Drops the current response; process() then reports it did nothing. */
    void clear();

    bool hasImpulseResponse() const noexcept { return loaded.load (std::memory_order_relaxed); }

    /** Name of the loaded file without extension, or empty. */
    juce::String getLoadedName() const;

    /**
     * Convolves the buffer in place.
     * @return false when there is nothing loaded and the buffer is untouched.
     */
    bool process (juce::AudioBuffer<float>& buffer);

private:
    // Shared by every Convolution instance; must outlive them, so it is declared
    // before the engine that uses it.
    juce::dsp::ConvolutionMessageQueue queue;
    std::unique_ptr<juce::dsp::Convolution> convolution;

    std::atomic<bool> loaded { false };
    bool prepared = false;

    juce::CriticalSection nameLock;
    juce::String loadedName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IrEngine)
};
} // namespace milodikfx::dsp
