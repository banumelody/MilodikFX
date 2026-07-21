#pragma once

#include <JuceHeader.h>

#include <atomic>

#include "dsp/AudioProcessorBase.h"
#include "dsp/Biquad.h"

namespace milodikfx::dsp
{
/**
 * Matches the guitar's output level to the chain: -24..+24 dB.
 *
 * First in the chain, *before* the noise gate, and that ordering is the whole
 * point. With the trim in front, the gate threshold stays correct relative to
 * the signal, so swapping guitars means re-dialling one knob rather than two.
 * Behind the gate the threshold would be tied to raw interface level instead.
 *
 * This is why the clean boost cannot double as a trim: it can only add gain,
 * and it sits after the gate.
 *
 * Always in the path, so there is no enable switch. At 0 dB it is a true
 * bit-identical passthrough rather than a multiply by 1.0f.
 */
class InputTrimProcessor final : public AudioProcessorBase
{
public:
    static constexpr float kMinDb = -24.0f;
    static constexpr float kMaxDb = 24.0f;

    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels) override;
    void processBlock (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setGainDb (float db) noexcept;
    float getGainDb() const noexcept;

private:
    std::atomic<float> gainDb { 0.0f };
    std::atomic<float> gainLinear { 1.0f };
    bool prepared = false;

    SmoothedParam smoothedGain;
};
} // namespace milodikfx::dsp
