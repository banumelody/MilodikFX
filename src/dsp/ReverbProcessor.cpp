#include "ReverbProcessor.h"
#include <cmath>

namespace milodikfx::dsp {

// CombFilter implementation
void ReverbProcessor::CombFilter::setDamp (float damp)
{
    damp1 = damp;
    damp2 = 1.0f - damp;
}

void ReverbProcessor::CombFilter::setFeedback (float fb)
{
    feedback = fb;
}

float ReverbProcessor::CombFilter::process (float input)
{
    if (buffer.empty())
        return input;

    float bufOut = buffer[bufferIndex];
    filterStore = (bufOut * damp2) + (filterStore * damp1);
    buffer[bufferIndex] = input + (filterStore * feedback);

    bufferIndex = (bufferIndex + 1) % (int)buffer.size();
    return bufOut;
}

void ReverbProcessor::CombFilter::reset()
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    filterStore = 0.0f;
    bufferIndex = 0;
}

// AllpassFilter implementation
float ReverbProcessor::AllpassFilter::process (float input)
{
    if (buffer.empty())
        return input;

    float bufOut = buffer[bufferIndex];
    float output = -input + bufOut;
    buffer[bufferIndex] = input + (bufOut * feedback);

    bufferIndex = (bufferIndex + 1) % (int)buffer.size();
    return output;
}

void ReverbProcessor::AllpassFilter::reset()
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    bufferIndex = 0;
}

// ReverbProcessor implementation
void ReverbProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock, int numChannels)
{
    juce::ignoreUnused (samplesPerBlock, numChannels);
    sampleRate = sampleRateIn;

    // Initialize comb and allpass filters with tuned delay times
    for (int i = 0; i < kNumCombFilters; ++i)
    {
        int delayTime = (int)(kCombFilterTuning[i] * sampleRate / 44100.0);
        combL[i].buffer.resize (delayTime, 0.0f);
        combR[i].buffer.resize (delayTime, 0.0f);
        combL[i].setDamp (0.5f);
        combR[i].setDamp (0.5f);
    }

    for (int i = 0; i < kNumAllpassFilters; ++i)
    {
        int delayTime = (int)(kAllpassFilterTuning[i] * sampleRate / 44100.0);
        allpassL[i].buffer.resize (delayTime, 0.0f);
        allpassR[i].buffer.resize (delayTime, 0.0f);
        allpassL[i].setFeedback (0.5f);
        allpassR[i].setFeedback (0.5f);
    }

    updateParameters();
}

void ReverbProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (!enabled.load())
        return;

    updateParameters();

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float inputL = buffer.getNumChannels() > 0 ? buffer.getReadPointer (0)[i] : 0.0f;
        float inputR = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1)[i] : inputL;

        // Sum inputs
        float input = (inputL + inputR) * 0.5f;

        // Process through comb filters (parallel)
        float outL = 0.0f, outR = 0.0f;
        for (int j = 0; j < kNumCombFilters; ++j)
        {
            outL += combL[j].process (input);
            outR += combR[j].process (input);
        }

        // Process through allpass filters (series)
        for (int j = 0; j < kNumAllpassFilters; ++j)
        {
            outL = allpassL[j].process (outL);
            outR = allpassR[j].process (outR);
        }

        // Mix with stereo width
        float stereoWidth = outL - outR;
        outL = outL + (stereoWidth * width.load());
        outR = outR - (stereoWidth * width.load());

        // Dry/wet mix
        float dryL = inputL * dry;
        float dryR = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1)[i] * dry : inputL * dry;
        float wetL = outL * wet1 + outR * wet2;
        float wetR = outR * wet1 + outL * wet2;

        if (buffer.getNumChannels() > 0)
            buffer.getWritePointer (0)[i] = dryL + wetL;
        if (buffer.getNumChannels() > 1)
            buffer.getWritePointer (1)[i] = dryR + wetR;
    }
}

void ReverbProcessor::reset()
{
    for (auto& c : combL) c.reset();
    for (auto& c : combR) c.reset();
    for (auto& a : allpassL) a.reset();
    for (auto& a : allpassR) a.reset();
}

void ReverbProcessor::setRoomSize (float size) noexcept
{
    roomSize.store (juce::jlimit (0.0f, 1.0f, size));
    updateParameters();
}

void ReverbProcessor::setDryWetMix (float mix) noexcept
{
    dryWetMix.store (juce::jlimit (0.0f, 1.0f, mix));
    updateParameters();
}

void ReverbProcessor::setDecayTime (float seconds) noexcept
{
    decayTime.store (juce::jlimit (0.5f, 10.0f, seconds));
    updateParameters();
}

void ReverbProcessor::setWidth (float w) noexcept
{
    width.store (juce::jlimit (0.0f, 1.0f, w));
}

void ReverbProcessor::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable);
}

float ReverbProcessor::getRoomSize() const noexcept { return roomSize.load(); }
float ReverbProcessor::getDryWetMix() const noexcept { return dryWetMix.load(); }
float ReverbProcessor::getDecayTime() const noexcept { return decayTime.load(); }
float ReverbProcessor::getWidth() const noexcept { return width.load(); }
bool ReverbProcessor::isEnabled() const noexcept { return enabled.load(); }

void ReverbProcessor::updateParameters()
{
    dry = 1.0f - dryWetMix.load();
    wet1 = dryWetMix.load() * (1.0f - roomSize.load());
    wet2 = dryWetMix.load() * roomSize.load();

    float damp = 0.4f + (roomSize.load() * 0.3f);
    for (auto& c : combL) c.setDamp (damp);
    for (auto& c : combR) c.setDamp (damp);
}

} // namespace milodikfx::dsp
