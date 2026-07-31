#include "dsp/MixerProcessor.h"

#include <cmath>

namespace milodikfx::dsp
{
namespace
{
/**
 * Constant-power pan gains for one path.
 *
 * -1 hard left through +1 hard right, sin/cos law so the perceived level holds
 * steady across the sweep. Linear panning would dip 3 dB in the middle, and with
 * `mixer.panA/B` available as a modifier target that dip would be an audible
 * pump on every auto-pan cycle.
 */
inline void panGains (float pan, float& left, float& right) noexcept
{
    const auto angle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;

    left = std::cos (angle);
    right = std::sin (angle);
}
} // namespace

void MixerProcessor::prepareToPlay (double, int, int) {}

void MixerProcessor::reset() {}

void MixerProcessor::processBlock (juce::AudioBuffer<float>&)
{
    // Nothing on its own: the mix point is a place in the chain, and
    // DSPChainManager calls combine() when it reaches this stage.
}

void MixerProcessor::combine (juce::AudioBuffer<float>& pathA, const juce::AudioBuffer<float>& pathB) noexcept
{
    const auto numSamples = juce::jmin (pathA.getNumSamples(), pathB.getNumSamples());
    const auto numChannels = juce::jmin (juce::jmin (pathA.getNumChannels(), pathB.getNumChannels()),
                                         kMaxChannels);

    if (numSamples <= 0 || numChannels <= 0)
        return;

    const auto gainA = levelA.load (std::memory_order_relaxed);

    // Polarity folds into B's gain rather than costing a branch per sample.
    const auto gainB = levelB.load (std::memory_order_relaxed)
                     * (invertB.load (std::memory_order_relaxed) ? -1.0f : 1.0f);

    float aLeft = 1.0f, aRight = 1.0f, bLeft = 1.0f, bRight = 1.0f;
    panGains (panA.load (std::memory_order_relaxed), aLeft, aRight);
    panGains (panB.load (std::memory_order_relaxed), bLeft, bRight);

    // Centre is 1/sqrt(2) per side under a constant-power law, so both paths
    // dead centre would come out 3 dB down against the serial chain. Scaling by
    // sqrt(2) makes "split with everything centred" match "no split at all",
    // which is what anyone comparing the two will expect.
    constexpr auto centreCompensation = juce::MathConstants<float>::sqrt2;

    if (numChannels == 1)
    {
        // Mono out: pan has nowhere to go, so only the levels matter.
        auto* a = pathA.getWritePointer (0);
        const auto* b = pathB.getReadPointer (0);

        for (int i = 0; i < numSamples; ++i)
            a[i] = a[i] * gainA + b[i] * gainB;

        return;
    }

    auto* left = pathA.getWritePointer (0);
    auto* right = pathA.getWritePointer (1);
    const auto* bLeftIn = pathB.getReadPointer (0);
    const auto* bRightIn = pathB.getReadPointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto aL = left[i];
        const auto aR = right[i];

        left[i] = (aL * aLeft * gainA + bLeftIn[i] * bLeft * gainB) * centreCompensation;
        right[i] = (aR * aRight * gainA + bRightIn[i] * bRight * gainB) * centreCompensation;
    }
}
} // namespace milodikfx::dsp
