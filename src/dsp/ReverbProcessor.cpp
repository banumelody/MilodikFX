#include "ReverbProcessor.h"

#include <algorithm>
#include <cmath>

namespace milodikfx::dsp {

void ReverbProcessor::DelayLine::allocate (int tuningAt44100)
{
    // Sized for the fastest rate we support, so a rate change never reallocates.
    const auto maxLength = (int) std::ceil (tuningAt44100 * (kMaxSampleRate / 44100.0)) + 1;

    buffer.assign ((size_t) juce::jmax (1, maxLength), 0.0f);
    activeLength = (int) buffer.size();
    bufferIndex = 0;
}

void ReverbProcessor::DelayLine::setLengthForRate (double sampleRate)
{
    juce::ignoreUnused (sampleRate);
}

float ReverbProcessor::CombFilter::process (float input) noexcept
{
    // activeLength can only shrink within an allocation that never moves, so a
    // stale value is still a safe index.
    const auto length = juce::jlimit (1, (int) buffer.size(), activeLength);

    if (buffer.empty())
        return input;

    if (bufferIndex < 0 || bufferIndex >= length)
        bufferIndex = 0;

    const auto bufOut = buffer[(size_t) bufferIndex];
    filterStore = (bufOut * damp2) + (filterStore * damp1);

    auto stored = input + (filterStore * feedback);

    if (! std::isfinite (stored))
        stored = 0.0f;

    buffer[(size_t) bufferIndex] = stored;

    if (++bufferIndex >= length)
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
    const auto length = juce::jlimit (1, (int) buffer.size(), activeLength);

    if (buffer.empty())
        return input;

    if (bufferIndex < 0 || bufferIndex >= length)
        bufferIndex = 0;

    const auto bufOut = buffer[(size_t) bufferIndex];
    const auto output = -input + bufOut;

    auto stored = input + (bufOut * feedback);

    if (! std::isfinite (stored))
        stored = 0.0f;

    buffer[(size_t) bufferIndex] = stored;

    if (++bufferIndex >= length)
        bufferIndex = 0;

    return output;
}

void ReverbProcessor::AllpassFilter::reset() noexcept
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    bufferIndex = 0;
}

ReverbProcessor::ReverbProcessor()
{
    // Allocate every line once, for the highest rate we support. After this
    // nothing here ever reallocates, so a device change cannot free memory the
    // audio thread is reading.
    for (int i = 0; i < kNumCombFilters; ++i)
    {
        combL[(size_t) i].allocate (kCombFilterTuning[i]);
        combR[(size_t) i].allocate (kCombFilterTuning[i] + kStereoSpread);
    }

    for (int i = 0; i < kNumAllpassFilters; ++i)
    {
        allpassL[(size_t) i].allocate (kAllpassFilterTuning[i]);
        allpassR[(size_t) i].allocate (kAllpassFilterTuning[i] + kStereoSpread);
    }

    // Same discipline as the delay lines: sized once, never resized.
    irDryCopy.setSize (kMaxIrChannels, kMaxIrBlock, false, true, false);
}

void ReverbProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock, int numChannels)
{
    sampleRate = sampleRateIn > 0.0 ? sampleRateIn : 44100.0;

    irEngine.prepare (sampleRate,
                      juce::jmax (1, samplesPerBlock),
                      juce::jlimit (1, kMaxIrChannels, numChannels));

    const auto scale = sampleRate / 44100.0;

    auto setLength = [] (DelayLine& line, int tuningAt44100, double rateScale)
    {
        const auto wanted = juce::jmax (1, (int) (tuningAt44100 * rateScale));
        line.activeLength = juce::jlimit (1, (int) line.buffer.size(), wanted);
    };

    for (int i = 0; i < kNumCombFilters; ++i)
    {
        setLength (combL[(size_t) i], kCombFilterTuning[i], scale);
        setLength (combR[(size_t) i], kCombFilterTuning[i] + kStereoSpread, scale);
    }

    for (int i = 0; i < kNumAllpassFilters; ++i)
    {
        setLength (allpassL[(size_t) i], kAllpassFilterTuning[i], scale);
        setLength (allpassR[(size_t) i], kAllpassFilterTuning[i] + kStereoSpread, scale);
        allpassL[(size_t) i].setFeedback (0.5f);
        allpassR[(size_t) i].setFeedback (0.5f);
    }

    activeStep = (float) (1.0 / juce::jmax (1.0, sampleRate * (double) kSpilloverFadeSeconds));

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

        const auto delaySecondsL = (float) (l.activeLength / sampleRate);
        const auto delaySecondsR = (float) (r.activeLength / sampleRate);

        l.setFeedback (juce::jlimit (0.0f, 0.97f, std::pow (10.0f, -3.0f * delaySecondsL / decaySeconds)));
        r.setFeedback (juce::jlimit (0.0f, 0.97f, std::pow (10.0f, -3.0f * delaySecondsR / decaySeconds)));

        l.setDamp (damp);
        r.setDamp (damp);
    }
}

void ReverbProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    const auto wantActive = enabled.load (std::memory_order_relaxed);

    if (! spillover.load (std::memory_order_relaxed))
    {
        // Hard switch, exactly as it behaved before spillover existed.
        if (! wantActive)
        {
            activeAmount = 0.0f;
            tailIdle = true;
            return;
        }

        activeAmount = 1.0f;
    }
    else if (! wantActive && tailIdle)
    {
        // Switched off and the room has already decayed to nothing.
        return;
    }

    if (wantActive)
    {
        // Instant on, exactly as before spillover existed, so an enabled reverb
        // is bit-identical to the old code. Only the fade out is new.
        activeAmount = 1.0f;
        tailIdle = false;
    }

    updateParametersIfNeeded();

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    if (numChannels <= 0 || numSamples <= 0 || combL[0].buffer.empty())
        return;

    // Convolution mode: the IR carries the room, so only the dry/wet balance is
    // applied here. Falls through to the algorithmic path when nothing is loaded.
    if (useImpulseResponse.load (std::memory_order_relaxed) && irEngine.hasImpulseResponse())
    {
        const auto irChannels = juce::jmin (numChannels, kMaxIrChannels);

        if (numSamples <= irDryCopy.getNumSamples() && irChannels > 0)
        {
            for (int ch = 0; ch < irChannels; ++ch)
                irDryCopy.copyFrom (ch, 0, buffer, ch, 0, numSamples);

            // Where the fade will have reached by the end of this block. The
            // convolution takes a ramp rather than a per-sample factor because
            // it consumes the whole buffer in one call.
            const auto startActive = activeAmount;
            const auto endActive = wantActive
                                       ? 1.0f
                                       : juce::jmax (0.0f, activeAmount - activeStep * (float) numSamples);

            if (startActive < 1.0f || endActive < 1.0f)
                for (int ch = 0; ch < irChannels; ++ch)
                    buffer.applyGainRamp (ch, 0, numSamples, startActive, endActive);

            if (irEngine.process (buffer))
            {
                const auto mix = dryWetMix.load (std::memory_order_relaxed);
                auto* const* wet = buffer.getArrayOfWritePointers();

                const auto activeStepPerSample = numSamples > 0
                                                     ? (endActive - startActive) / (float) numSamples
                                                     : 0.0f;

                auto tailPeak = 0.0f;

                for (int ch = 0; ch < irChannels; ++ch)
                {
                    for (int i = 0; i < numSamples; ++i)
                    {
                        const auto active = startActive + activeStepPerSample * (float) i;

                        tailPeak = juce::jmax (tailPeak, std::abs (wet[ch][i]));

                        wet[ch][i] = irDryCopy.getSample (ch, i) * (1.0f - mix * active)
                                     + wet[ch][i] * mix;
                    }
                }

                activeAmount = endActive;

                if (activeAmount <= 0.0f && tailPeak < kTailSilence)
                    tailIdle = true;

                return;
            }

            // Undo the fade before handing over: the algorithmic path applies
            // its own, and a doubly-faded input would decay twice as fast.
            if (startActive < 1.0f || endActive < 1.0f)
                for (int ch = 0; ch < irChannels; ++ch)
                    buffer.copyFrom (ch, 0, irDryCopy, ch, 0, numSamples);

            // Convolution declined the block, so put the dry signal back before
            // handing over to the algorithmic path.
            for (int ch = 0; ch < irChannels; ++ch)
                buffer.copyFrom (ch, 0, irDryCopy, ch, 0, numSamples);
        }
    }

    auto* const* channels = buffer.getArrayOfWritePointers();
    const auto hasRight = numChannels > 1;

    auto tailPeak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        // Faded rather than switched: the dry gain moving back to full would
        // otherwise step.
        if (! wantActive)
            activeAmount = juce::jmax (0.0f, activeAmount - activeStep);

        const auto inputL = channels[0][i];
        const auto inputR = hasRight ? channels[1][i] : inputL;

        // Only the feed is faded; the comb and allpass network keeps running,
        // which is what lets the room ring on instead of stopping dead.
        const auto input = (inputL + inputR) * kFixedGain * activeAmount;

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

        tailPeak = juce::jmax (tailPeak, std::abs (outL), std::abs (outR));

        // Dry returns to full as the reverb fades out, so switching off
        // restores the unaffected signal rather than leaving it attenuated.
        const auto dryGain = 1.0f - (1.0f - dry) * activeAmount;

        channels[0][i] = outL * wet1 + outR * wet2 + inputL * dryGain;

        if (hasRight)
            channels[1][i] = outR * wet1 + outL * wet2 + inputR * dryGain;
    }

    if (activeAmount <= 0.0f && tailPeak < kTailSilence)
        tailIdle = true;
}

void ReverbProcessor::reset()
{
    for (auto& c : combL) c.reset();
    for (auto& c : combR) c.reset();
    for (auto& a : allpassL) a.reset();
    for (auto& a : allpassR) a.reset();

    irEngine.reset();

    const auto on = enabled.load (std::memory_order_relaxed);
    activeAmount = on ? 1.0f : 0.0f;
    tailIdle = ! on;
}

void ReverbProcessor::setSpillover (bool shouldSpill) noexcept
{
    spillover.store (shouldSpill, std::memory_order_relaxed);
}

bool ReverbProcessor::isSpillover() const noexcept
{
    return spillover.load (std::memory_order_relaxed);
}

void ReverbProcessor::setUseImpulseResponse (bool shouldUseIr) noexcept
{
    useImpulseResponse.store (shouldUseIr, std::memory_order_relaxed);
}

bool ReverbProcessor::isUsingImpulseResponse() const noexcept
{
    return useImpulseResponse.load (std::memory_order_relaxed);
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
