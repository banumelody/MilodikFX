#include "dsp/SplitProcessor.h"

namespace milodikfx::dsp
{
void SplitProcessor::prepareToPlay (double sampleRateIn, int, int)
{
    sampleRate = sampleRateIn > 0.0 ? sampleRateIn : 48000.0;

    // Force a rebuild: fresh filters must be given coefficients explicitly, or
    // they run flat until a parameter happens to move. The EQ shipped that bug
    // once and went silent after every device restart.
    builtForFrequency = -1.0f;
    builtForRate = 0.0;

    reset();
    rebuildIfNeeded();
}

void SplitProcessor::reset()
{
    for (auto& channel : lowStates)
        for (auto& section : channel)
            section.reset();

    for (auto& channel : highStates)
        for (auto& section : channel)
            section.reset();
}

void SplitProcessor::setFrequencyHz (float hz) noexcept
{
    frequencyHz.store (juce::jlimit (kMinFrequencyHz, kMaxFrequencyHz, hz), std::memory_order_relaxed);
}

void SplitProcessor::rebuildIfNeeded() noexcept
{
    const auto wanted = frequencyHz.load (std::memory_order_relaxed);

    if (wanted == builtForFrequency && sampleRate == builtForRate)
        return;

    // Butterworth Q for each of the two cascaded sections; two of them in series
    // make a Linkwitz-Riley 4th order, whose low and high halves sum back to
    // flat magnitude. A single section per side would leave a 3 dB bump at the
    // crossover, which on a bass rig is exactly where it would be noticed.
    constexpr double butterworthQ = 0.70710678;

    lowCoeffs = biquad::makeLowPass (sampleRate, wanted, butterworthQ);
    highCoeffs = biquad::makeHighPass (sampleRate, wanted, butterworthQ);

    builtForFrequency = wanted;
    builtForRate = sampleRate;
}

void SplitProcessor::processBlock (juce::AudioBuffer<float>&)
{
    // Nothing: the split is a place in the chain, not a thing done to the
    // signal. DSPChainManager recognises this stage and calls divide().
}

void SplitProcessor::divide (juce::AudioBuffer<float>& pathA, juce::AudioBuffer<float>& pathB) noexcept
{
    const auto numSamples = juce::jmin (pathA.getNumSamples(), pathB.getNumSamples());
    const auto numChannels = juce::jmin (juce::jmin (pathA.getNumChannels(), pathB.getNumChannels()),
                                         kMaxChannels);

    if (numSamples <= 0 || numChannels <= 0)
        return;

    if ((Mode) splitMode.load (std::memory_order_relaxed) == Mode::even)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            pathB.copyFrom (ch, 0, pathA, ch, 0, numSamples);

        return;
    }

    rebuildIfNeeded();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* a = pathA.getWritePointer (ch);
        auto* b = pathB.getWritePointer (ch);

        auto& low = lowStates[(size_t) ch];
        auto& high = highStates[(size_t) ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const auto in = a[i];

            auto lowOut = in;
            auto highOut = in;

            for (int section = 0; section < kSections; ++section)
            {
                lowOut = low[(size_t) section].process (lowCoeffs, lowOut);
                highOut = high[(size_t) section].process (highCoeffs, highOut);
            }

            // Guard: a filter that has been handed a non-finite sample would
            // latch it forever, and this sits well before the master limiter.
            a[i] = std::isfinite (lowOut) ? lowOut : 0.0f;
            b[i] = std::isfinite (highOut) ? highOut : 0.0f;
        }
    }
}
} // namespace milodikfx::dsp
