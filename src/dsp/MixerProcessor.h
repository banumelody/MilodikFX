#pragma once

#include <JuceHeader.h>

#include <atomic>

#include "dsp/AudioProcessorBase.h"

namespace milodikfx::dsp
{
/**
 * Where the two paths become one again.
 *
 * Level and pan for each bus, so path A can sit hard left and path B hard right
 * -- which is how a stereo rig is actually built -- or both can go to the centre
 * and the split simply becomes a parallel blend. Pedalboard's Mixer does exactly
 * this, and Fractal's Output block does it per grid row.
 *
 * Like the split, this is a stage in the chain rather than a property of it, so
 * dragging it decides where the parallel section ends. It does not process the
 * signal on its own; `DSPChainManager` recognises it and hands it both buffers.
 *
 * Pan is constant power (-3 dB at centre), so sweeping a path across the image
 * does not change how loud it is -- which matters because `mixer.panA/B` is a
 * legitimate modifier target, and an auto-panner that pumped would be useless.
 */
class MixerProcessor final : public AudioProcessorBase
{
public:
    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels) override;
    void processBlock (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setLevelA (float linear) noexcept { levelA.store (juce::jlimit (0.0f, 2.0f, linear), std::memory_order_relaxed); }
    float getLevelA() const noexcept { return levelA.load (std::memory_order_relaxed); }

    void setLevelB (float linear) noexcept { levelB.store (juce::jlimit (0.0f, 2.0f, linear), std::memory_order_relaxed); }
    float getLevelB() const noexcept { return levelB.load (std::memory_order_relaxed); }

    /** -1 hard left, 0 centre, +1 hard right. */
    void setPanA (float pan) noexcept { panA.store (juce::jlimit (-1.0f, 1.0f, pan), std::memory_order_relaxed); }
    float getPanA() const noexcept { return panA.load (std::memory_order_relaxed); }

    void setPanB (float pan) noexcept { panB.store (juce::jlimit (-1.0f, 1.0f, pan), std::memory_order_relaxed); }
    float getPanB() const noexcept { return panB.load (std::memory_order_relaxed); }

    /**
     * Folds `pathB` into `pathA` with each bus's level and pan applied.
     * Called by DSPChainManager on the audio thread at the mix point.
     */
    void combine (juce::AudioBuffer<float>& pathA, const juce::AudioBuffer<float>& pathB) noexcept;

private:
    static constexpr int kMaxChannels = 2;

    std::atomic<float> levelA { 1.0f };
    std::atomic<float> levelB { 1.0f };
    std::atomic<float> panA { 0.0f };
    std::atomic<float> panB { 0.0f };
};
} // namespace milodikfx::dsp
