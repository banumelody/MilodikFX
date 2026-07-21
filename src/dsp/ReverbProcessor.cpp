#include "ReverbProcessor.h"

#include <algorithm>
#include <cmath>

namespace milodikfx::dsp {

float ReverbProcessor::CombFilter::process (float input) noexcept
{
    if (buffer.empty())
        return input;

    const auto bufOut = buffer[(size_t) bufferIndex];
    filterStore = (bufOut * damp2) + (filterStore * damp1);

    auto stored = input + (filterStore * feedback);

    if (! std::isfinite (stored))
        stored = 0.0f;

    buffer[(size_t) bufferIndex] = stored;

    if (++bufferIndex >= (int) buffer.size())
        bufferIndex = 0;

    return bufOut;
}

void ReverbProcessor::CombFilter::reset() noexcept
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    filterStore = 0.0f;
    bufferIndex = 0;
}

float ReverbProcessor::AllpassFilter::process (float input) noexcept
{
    if (buffer.empty())
        return input;

    const auto bufOut = buffer[(size_t) bufferIndex];
    const auto output = -input + bufOut;

    auto stored = input + (bufOut * feedback);

    if (! std::isfinite (stored))
        stored = 0.0f;

    buffer[(size_t) bufferIndex] = stored;

    if (++bufferIndex >= (int) buffer.size())
        bufferIndex = 0;

    return output;
}

void ReverbProcessor::AllpassFilter::reset() noexcept
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    bufferIndex = 0;
}

void ReverbProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock, int numChannels)
{
    juce::ignoreUnused (samplesPerBlock, numChannels);
    sampleRate = sampleRateIn > 0.0 ? sampleRateIn : 44100.0;

    const auto scale = sampleRate / 44100.0;

    for (int i = 0; i < kNumCombFilters; ++i)
    {
        const auto lengthL = juce::jmax (1, (int) (kCombFilterTuning[i] * scale));
        const auto lengthR = juce::jmax (1, (int) ((kCombFilterTuning[i] + kStereoSpread) * scale));

        combL[(size_t) i].buffer.assign ((size_t) lengthL, 0.0f);
        combR[(size_t) i].buffer.assign ((size_t) lengthR, 0.0f);
    }

    for (int i = 0; i < kNumAllpassFilters; ++i)
    {
        const auto lengthL = juce::jmax (1, (int) (kAllpassFilterTuning[i] * scale));
        const auto lengthR = juce::jmax (1, (int) ((kAllpassFilterTuning[i] + kStereoSpread) * scale));

        allpassL[(size_t) i].buffer.assign ((size_t) lengthL, 0.0f);
        allpassR[(size_t) i].buffer.assign ((size_t) lengthR, 0.0f);
        allpassL[(size_t) i].setFeedback (0.5f);
        allpassR[(size_t) i].setFeedback (0.5f);
    }

    reset();

    // Delay lengths just changed, so the RT60-derived feedbacks must be redone.
    parametersDirty.store (true, std::memory_order_relaxed);
    updateParametersIfNeeded();
}

void ReverbProcessor::updateParametersIfNeeded() noexcept
{
    if (! parametersDirty.exchange (false, std::memory_order_relaxed))
        return;

    const auto mix = dryWetMix.load (std::memory_order_relaxed);
    const auto room = roomSize.load (std::memory_order_relaxed);
    const auto w = width.load (std::memory_order_relaxed);

    dry = 1.0f - mix;

    const auto wet = mix * kWetScale;
    wet1 = wet * (w * 0.5f + 0.5f);
    wet2 = wet * ((1.0f - w) * 0.5f);

    // Room size stretches the tail on top of the explicit decay setting so both
    // knobs are audible, then RT60 gives each comb its own feedback.
    const auto decaySeconds = juce::jmax (0.1f, decayTime.load (std::memory_order_relaxed) * (0.5f + room));
    const auto damp = juce::jlimit (0.05f, 0.9f, 0.5f - room * 0.3f);

    for (int i = 0; i < kNumCombFilters; ++i)
    {
        auto& l = combL[(size_t) i];
        auto& r = combR[(size_t) i];

        const auto delaySecondsL = (float) (l.buffer.size() / sampleRate);
        const auto delaySecondsR = (float) (r.buffer.size() / sampleRate);

        l.setFeedback (juce::jlimit (0.0f, 0.97f, std::pow (10.0f, -3.0f * delaySecondsL / decaySeconds)));
        r.setFeedback (juce::jlimit (0.0f, 0.97f, std::pow (10.0f, -3.0f * delaySecondsR / decaySeconds)));

        l.setDamp (damp);
        r.setDamp (damp);
    }
}

void ReverbProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (! enabled.load (std::memory_order_relaxed))
        return;

    updateParametersIfNeeded();

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    if (numChannels <= 0 || numSamples <= 0 || combL[0].buffer.empty())
        return;

    auto* const* channels = buffer.getArrayOfWritePointers();
    const auto hasRight = numChannels > 1;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto inputL = channels[0][i];
        const auto inputR = hasRight ? channels[1][i] : inputL;

        const auto input = (inputL + inputR) * kFixedGain;

        float outL = 0.0f;
        float outR = 0.0f;

        for (int j = 0; j < kNumCombFilters; ++j)
        {
            outL += combL[(size_t) j].process (input);
            outR += combR[(size_t) j].process (input);
        }

        for (int j = 0; j < kNumAllpassFilters; ++j)
        {
            outL = allpassL[(size_t) j].process (outL);
            outR = allpassR[(size_t) j].process (outR);
        }

        channels[0][i] = outL * wet1 + outR * wet2 + inputL * dry;

        if (hasRight)
            channels[1][i] = outR * wet1 + outL * wet2 + inputR * dry;
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
    roomSize.store (juce::jlimit (0.0f, 1.0f, size), std::memory_order_relaxed);
    parametersDirty.store (true, std::memory_order_relaxed);
}

void ReverbProcessor::setDryWetMix (float mix) noexcept
{
    dryWetMix.store (juce::jlimit (0.0f, 1.0f, mix), std::memory_order_relaxed);
    parametersDirty.store (true, std::memory_order_relaxed);
}

void ReverbProcessor::setDecayTime (float seconds) noexcept
{
    decayTime.store (juce::jlimit (0.2f, 10.0f, seconds), std::memory_order_relaxed);
    parametersDirty.store (true, std::memory_order_relaxed);
}

void ReverbProcessor::setWidth (float w) noexcept
{
    width.store (juce::jlimit (0.0f, 1.0f, w), std::memory_order_relaxed);
    parametersDirty.store (true, std::memory_order_relaxed);
}

void ReverbProcessor::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable, std::memory_order_relaxed);
}

float ReverbProcessor::getRoomSize() const noexcept { return roomSize.load (std::memory_order_relaxed); }
float ReverbProcessor::getDryWetMix() const noexcept { return dryWetMix.load (std::memory_order_relaxed); }
float ReverbProcessor::getDecayTime() const noexcept { return decayTime.load (std::memory_order_relaxed); }
float ReverbProcessor::getWidth() const noexcept { return width.load (std::memory_order_relaxed); }
bool ReverbProcessor::isEnabled() const noexcept { return enabled.load (std::memory_order_relaxed); }

} // namespace milodikfx::dsp
