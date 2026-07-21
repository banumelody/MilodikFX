#include "dsp/IrEngine.h"

namespace milodikfx::dsp
{
IrEngine::IrEngine()
{
    convolution = std::make_unique<juce::dsp::Convolution> (queue);
}

IrEngine::~IrEngine() = default;

void IrEngine::prepare (double sampleRate, int maximumBlockSize, int numChannels)
{
    juce::dsp::ProcessSpec spec {};
    spec.sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    spec.maximumBlockSize = (juce::uint32) juce::jmax (1, maximumBlockSize);
    spec.numChannels = (juce::uint32) juce::jmax (1, numChannels);

    convolution->prepare (spec);
    prepared = true;
}

void IrEngine::reset()
{
    if (convolution != nullptr)
        convolution->reset();
}

bool IrEngine::loadFromFile (const juce::File& file)
{
    if (! file.existsAsFile())
    {
        clear();
        return false;
    }

    // Reject anything the audio format manager cannot open before handing it to
    // the convolution, which would otherwise silently keep the previous IR and
    // leave the UI claiming a file is loaded when it is not.
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr || reader->lengthInSamples <= 0)
    {
        clear();
        return false;
    }

    reader.reset();

    convolution->loadImpulseResponse (file,
                                      juce::dsp::Convolution::Stereo::yes,
                                      juce::dsp::Convolution::Trim::yes,
                                      0,
                                      juce::dsp::Convolution::Normalise::yes);

    {
        const juce::ScopedLock lock (nameLock);
        loadedName = file.getFileNameWithoutExtension();
    }

    loaded.store (true, std::memory_order_relaxed);
    return true;
}

void IrEngine::clear()
{
    loaded.store (false, std::memory_order_relaxed);

    const juce::ScopedLock lock (nameLock);
    loadedName = {};
}

juce::String IrEngine::getLoadedName() const
{
    const juce::ScopedLock lock (nameLock);
    return loadedName;
}

bool IrEngine::process (juce::AudioBuffer<float>& buffer)
{
    if (! prepared || ! loaded.load (std::memory_order_relaxed) || convolution == nullptr)
        return false;

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return false;

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    convolution->process (context);
    return true;
}
} // namespace milodikfx::dsp
