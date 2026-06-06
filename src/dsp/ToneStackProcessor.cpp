#include "ToneStackProcessor.h"
#include <cmath>

namespace milodikfx::dsp {

void ToneStackProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock, int numChannels)
{
    juce::ignoreUnused (samplesPerBlock);
    sampleRate = sampleRateIn;

    // Initialize biquad state for each channel
    bassFilter.states.resize (numChannels);
    midFilter.states.resize (numChannels);
    trebleFilter.states.resize (numChannels);

    updateBassCoefficients();
    updateMidCoefficients();
    updateTrebleCoefficients();
}

void ToneStackProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (!enabled.load())
        return;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& bassSt = bassFilter.states[ch];
        auto& midSt = midFilter.states[ch];
        auto& trebleSt = trebleFilter.states[ch];

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            // Process through bass filter
            float sample = processBiquad (data[i], bassSt.x1, bassSt.x2, bassSt.y1, bassSt.y2,
                                         bassFilter.b0, bassFilter.b1, bassFilter.b2,
                                         bassFilter.a1, bassFilter.a2);

            // Process through mid filter
            sample = processBiquad (sample, midSt.x1, midSt.x2, midSt.y1, midSt.y2,
                                   midFilter.b0, midFilter.b1, midFilter.b2,
                                   midFilter.a1, midFilter.a2);

            // Process through treble filter
            sample = processBiquad (sample, trebleSt.x1, trebleSt.x2, trebleSt.y1, trebleSt.y2,
                                   trebleFilter.b0, trebleFilter.b1, trebleFilter.b2,
                                   trebleFilter.a1, trebleFilter.a2);

            data[i] = sample;
        }
    }
}

void ToneStackProcessor::reset()
{
    for (auto& st : bassFilter.states)
    {
        st.x1 = st.x2 = st.y1 = st.y2 = 0.0f;
    }
    for (auto& st : midFilter.states)
    {
        st.x1 = st.x2 = st.y1 = st.y2 = 0.0f;
    }
    for (auto& st : trebleFilter.states)
    {
        st.x1 = st.x2 = st.y1 = st.y2 = 0.0f;
    }
}

float ToneStackProcessor::processBiquad (float input, float& state_x1, float& state_x2, float& state_y1, float& state_y2,
                                         float b0, float b1, float b2, float a1, float a2)
{
    float output = b0 * input + b1 * state_x1 + b2 * state_x2 - a1 * state_y1 - a2 * state_y2;

    state_x2 = state_x1;
    state_x1 = input;
    state_y2 = state_y1;
    state_y1 = output;

    return output;
}

void ToneStackProcessor::setBassDb (float db) noexcept
{
    bassDb.store (juce::jlimit (-12.0f, 12.0f, db));
    updateBassCoefficients();
}

void ToneStackProcessor::setMidDb (float db) noexcept
{
    midDb.store (juce::jlimit (-12.0f, 12.0f, db));
    updateMidCoefficients();
}

void ToneStackProcessor::setTrebleDb (float db) noexcept
{
    trebleDb.store (juce::jlimit (-12.0f, 12.0f, db));
    updateTrebleCoefficients();
}

void ToneStackProcessor::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable);
}

float ToneStackProcessor::getBassDb() const noexcept { return bassDb.load(); }
float ToneStackProcessor::getMidDb() const noexcept { return midDb.load(); }
float ToneStackProcessor::getTrebleDb() const noexcept { return trebleDb.load(); }
bool ToneStackProcessor::isEnabled() const noexcept { return enabled.load(); }

void ToneStackProcessor::updateBassCoefficients()
{
    // Peaking filter @ 50 Hz, Q=0.7, gain=bassDb
    const float centerFreq = 50.0f;
    const float Q = 0.7f;
    const float A = std::pow (10.0f, bassDb.load() / 40.0f);
    const float w0 = 2.0f * juce::MathConstants<float>::pi * centerFreq / (float)sampleRate;
    const float sinW0 = std::sin (w0);
    const float cosW0 = std::cos (w0);
    const float alpha = sinW0 / (2.0f * Q);

    bassFilter.b0 = 1.0f + alpha * A;
    bassFilter.b1 = -2.0f * cosW0;
    bassFilter.b2 = 1.0f - alpha * A;
    float a0 = 1.0f + alpha / A;
    bassFilter.a1 = -2.0f * cosW0 / a0;
    bassFilter.a2 = (1.0f - alpha / A) / a0;
    bassFilter.b0 /= a0;
    bassFilter.b1 /= a0;
    bassFilter.b2 /= a0;
}

void ToneStackProcessor::updateMidCoefficients()
{
    // Peaking filter @ 500 Hz, Q=0.7, gain=midDb
    const float centerFreq = 500.0f;
    const float Q = 0.7f;
    const float A = std::pow (10.0f, midDb.load() / 40.0f);
    const float w0 = 2.0f * juce::MathConstants<float>::pi * centerFreq / (float)sampleRate;
    const float sinW0 = std::sin (w0);
    const float cosW0 = std::cos (w0);
    const float alpha = sinW0 / (2.0f * Q);

    midFilter.b0 = 1.0f + alpha * A;
    midFilter.b1 = -2.0f * cosW0;
    midFilter.b2 = 1.0f - alpha * A;
    float a0 = 1.0f + alpha / A;
    midFilter.a1 = -2.0f * cosW0 / a0;
    midFilter.a2 = (1.0f - alpha / A) / a0;
    midFilter.b0 /= a0;
    midFilter.b1 /= a0;
    midFilter.b2 /= a0;
}

void ToneStackProcessor::updateTrebleCoefficients()
{
    // Peaking filter @ 5 kHz, Q=0.7, gain=trebleDb
    const float centerFreq = 5000.0f;
    const float Q = 0.7f;
    const float A = std::pow (10.0f, trebleDb.load() / 40.0f);
    const float w0 = 2.0f * juce::MathConstants<float>::pi * centerFreq / (float)sampleRate;
    const float sinW0 = std::sin (w0);
    const float cosW0 = std::cos (w0);
    const float alpha = sinW0 / (2.0f * Q);

    trebleFilter.b0 = 1.0f + alpha * A;
    trebleFilter.b1 = -2.0f * cosW0;
    trebleFilter.b2 = 1.0f - alpha * A;
    float a0 = 1.0f + alpha / A;
    trebleFilter.a1 = -2.0f * cosW0 / a0;
    trebleFilter.a2 = (1.0f - alpha / A) / a0;
    trebleFilter.b0 /= a0;
    trebleFilter.b1 /= a0;
    trebleFilter.b2 /= a0;
}

} // namespace milodikfx::dsp
