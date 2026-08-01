#pragma once

#include <JuceHeader.h>

#include <array>
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
 * **One trim per channel, linked by default.** With two sources on two jacks --
 * a magnetic pickup on one and a piezo on the other -- a single figure can only
 * ever match one of them to the chain, which is precisely the job this stage
 * exists to do. Linked is the default, so a mono rig and every preset written
 * before this behave exactly as they did.
 *
 * Always in the path, so there is no enable switch. At 0 dB it is a true
 * bit-identical passthrough rather than a multiply by 1.0f.
 */
class InputTrimProcessor final : public AudioProcessorBase
{
public:
    static constexpr float kMinDb = -24.0f;
    static constexpr float kMaxDb = 24.0f;
    static constexpr int kMaxChannels = 2;

    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels) override;
    void processBlock (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    /** The left trim, and the right one too while the two are linked. */
    void setGainDb (float db) noexcept;
    float getGainDb() const noexcept;

    /** Ignored while linked, but remembered, so unlinking restores what was set. */
    void setGainDbR (float db) noexcept;
    float getGainDbR() const noexcept;

    /** True: the right channel follows the left. Default, and what a mono rig wants. */
    void setLinked (bool shouldLink) noexcept;
    bool isLinked() const noexcept { return linked.load (std::memory_order_relaxed); }

    /** What a given channel is actually being trimmed by, in dB. */
    float getEffectiveGainDb (int channel) const noexcept;

private:
    void refreshTargets() noexcept;

    std::atomic<float> gainDb { 0.0f };
    std::atomic<float> gainDbRight { 0.0f };
    std::atomic<bool> linked { true };

    std::array<std::atomic<float>, kMaxChannels> gainLinear { { { 1.0f }, { 1.0f } } };
    bool prepared = false;

    std::array<SmoothedParam, kMaxChannels> smoothedGain;
};
} // namespace milodikfx::dsp
