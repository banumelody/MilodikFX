#include "dsp/OverdriveProcessor.h"

#include <cmath>

namespace milodikfx::dsp
{
void OverdriveProcessor::prepareToPlay (double, int, int)
{
    prepared = true;
}

float OverdriveProcessor::softClip (float x) noexcept
{
    const auto a = std::abs (x);

    if (a <= 1.0f)
        return x - (x * x * x) / 3.0f;

    return x > 0.0f ? (2.0f / 3.0f) : (-2.0f / 3.0f);
}

void OverdriveProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (! prepared)
        return;

    if (! enabled.load (std::memory_order_relaxed))
        return;

    const auto amount = driveAmount.load (std::memory_order_relaxed);
    const auto level = levelLinear.load (std::memory_order_relaxed);

    if (amount <= 0.0f)
    {
        if (level != 1.0f)
            buffer.applyGain (level);

        return;
    }

    if (amount >= 1.0f && level == 1.0f)
    {
        // fallthrough to full processing; the branch is only here to avoid extra checks in the loop.
    }

    // Pre-gain curve: 1x..20x (smoother at low drive).
    const auto driveGain = 1.0f + (amount * amount * 19.0f);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        const auto n = buffer.getNumSamples();

        for (int i = 0; i < n; ++i)
        {
            const auto x = data[i];

            // Cubic soft clip (scaled to hit +/-1.0 at saturation).
            const auto clipped = 1.5f * softClip (x * driveGain);

            // Blend from clean -> driven.
            auto y = ((1.0f - amount) * x) + (amount * clipped);
            y *= level;

            data[i] = y;
        }
    }
}

void OverdriveProcessor::reset()
{
}

void OverdriveProcessor::setDrivePercent (float percent) noexcept
{
    const auto clamped = juce::jlimit (0.0f, 100.0f, percent);
    drivePercent.store (clamped, std::memory_order_relaxed);
    driveAmount.store (clamped / 100.0f, std::memory_order_relaxed);
}

float OverdriveProcessor::getDrivePercent() const noexcept
{
    return drivePercent.load (std::memory_order_relaxed);
}

void OverdriveProcessor::setLevelPercent (float percent) noexcept
{
    const auto clamped = juce::jlimit (0.0f, 100.0f, percent);
    levelPercent.store (clamped, std::memory_order_relaxed);
    levelLinear.store (clamped / 100.0f, std::memory_order_relaxed);
}

float OverdriveProcessor::getLevelPercent() const noexcept
{
    return levelPercent.load (std::memory_order_relaxed);
}

void OverdriveProcessor::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable, std::memory_order_relaxed);
}

bool OverdriveProcessor::isEnabled() const noexcept
{
    return enabled.load (std::memory_order_relaxed);
}
} // namespace milodikfx::dsp
