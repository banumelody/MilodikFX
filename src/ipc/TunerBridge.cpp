#include "ipc/TunerBridge.h"
#include <cmath>

namespace milodikfx::ipc
{

TunerBridge::TunerBridge() = default;

juce::var TunerBridge::detectFrequency(const juce::AudioBuffer<float>& buffer, int sampleRate)
{
    // Simple autocorrelation-based frequency detection
    auto* audioData = buffer.getReadPointer(0);
    auto numSamples = buffer.getNumSamples();

    if (numSamples < 1024)
    {
        auto result = new juce::DynamicObject();
        result->setProperty("frequency", 0.0f);
        result->setProperty("note", "");
        result->setProperty("cents", 0.0f);
        return juce::var(result);
    }

    // Find fundamental frequency using autocorrelation
    float bestCorrelation = 0.0f;
    int bestLag = 0;

    int minPeriod = sampleRate / 400; // 400 Hz max
    int maxPeriod = sampleRate / 50;  // 50 Hz min

    for (int lag = minPeriod; lag < maxPeriod && lag < numSamples / 2; ++lag)
    {
        float correlation = 0.0f;
        for (int i = 0; i < numSamples - lag; ++i)
        {
            correlation += audioData[i] * audioData[i + lag];
        }

        if (correlation > bestCorrelation)
        {
            bestCorrelation = correlation;
            bestLag = lag;
        }
    }

    float frequency = 0.0f;
    if (bestLag > 0 && bestCorrelation > 0.1f)
    {
        frequency = static_cast<float>(sampleRate) / bestLag;
    }

    juce::String noteName = getNoteName(frequency);
    float cents = getCentsOff(frequency);

    auto result = new juce::DynamicObject();
    result->setProperty("frequency", frequency);
    result->setProperty("note", noteName);
    result->setProperty("cents", cents);
    return juce::var(result);
}

juce::String TunerBridge::getNoteName(float frequency)
{
    if (frequency < 20.0f)
        return "";

    int noteNumber = getNoteNumber(frequency);
    const char* notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int octave = (noteNumber / 12) - 1;
    int noteInOctave = noteNumber % 12;

    return juce::String(notes[noteInOctave]) + juce::String(octave);
}

float TunerBridge::getCentsOff(float frequency)
{
    if (frequency < 20.0f)
        return 0.0f;

    int noteNumber = getNoteNumber(frequency);
    float expectedFreq = A4_FREQUENCY * std::pow(2.0f, (noteNumber - 69) / 12.0f);
    float ratio = frequency / expectedFreq;
    return 1200.0f * std::log2(ratio);
}

int TunerBridge::getNoteNumber(float frequency)
{
    if (frequency < 20.0f)
        return 0;

    float semitones = 12.0f * std::log2(frequency / A4_FREQUENCY);
    return static_cast<int>(std::round(semitones + 69));
}

} // namespace milodikfx::ipc
