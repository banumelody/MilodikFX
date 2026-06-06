#pragma once

#include <JuceHeader.h>

namespace milodikfx::ipc
{

class TunerBridge
{
public:
    TunerBridge();

    // Frequency detection from audio buffer
    juce::var detectFrequency(const juce::AudioBuffer<float>& buffer, int sampleRate);

    // Get note name from frequency
    static juce::String getNoteName(float frequency);

    // Get cents off from nearest semitone
    static float getCentsOff(float frequency);

    // Get note number (0-127, where 60 = Middle C)
    static int getNoteNumber(float frequency);

private:
    static constexpr float A4_FREQUENCY = 440.0f;
};

} // namespace milodikfx::ipc
