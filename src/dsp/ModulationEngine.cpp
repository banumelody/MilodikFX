#include "dsp/ModulationEngine.h"

#include <cmath>

namespace milodikfx::dsp
{
namespace
{
/** An LFO shape mapped to 0..1, from a phase in [0, 1). */
float lfoValue (ModulationEngine::Source source, double phase) noexcept
{
    switch (source)
    {
        case ModulationEngine::Source::lfoTriangle:
            return (float) (phase < 0.5 ? phase * 2.0 : 2.0 - phase * 2.0);

        case ModulationEngine::Source::lfoSquare:
            return phase < 0.5 ? 1.0f : 0.0f;

        case ModulationEngine::Source::lfoSine:
        default:
            return (float) (0.5 + 0.5 * std::sin (juce::MathConstants<double>::twoPi * phase));
    }
}
} // namespace

void ModulationEngine::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    reset();
}

void ModulationEngine::reset() noexcept
{
    for (auto& slot : slots)
    {
        slot.phase = 0.0;
        slot.env = 0.0f;
        slot.wasActive = false;
    }
}

bool ModulationEngine::setModifier (int slot,
                                    const milodikfx::api::ParameterDescriptor* target,
                                    std::string effectId,
                                    std::string parameterId,
                                    Source source,
                                    float low,
                                    float high,
                                    float rateHz) noexcept
{
    if (! isValidSlot (slot) || target == nullptr || ! target->set || ! target->get
        || target->isText || target->isBoolean)
        return false;

    auto& s = slots[(size_t) slot];

    // Stop the audio thread touching this slot while it is reconfigured. A brief
    // overlap is harmless: it only ever changes config atomics and a published
    // pointer, and re-arms at the end.
    s.active.store (false, std::memory_order_release);

    s.effectId = std::move (effectId);
    s.parameterId = std::move (parameterId);

    s.source.store ((int) source, std::memory_order_relaxed);
    s.low.store (low, std::memory_order_relaxed);
    s.high.store (high, std::memory_order_relaxed);
    s.rateHz.store (juce::jmax (0.01f, rateHz), std::memory_order_relaxed);

    // The value to put back when the modifier is removed: what it is right now,
    // before modulation starts moving it.
    s.restoreValue.store (target->get(), std::memory_order_relaxed);

    // Publish the target, then re-arm. The audio thread reads active with acquire,
    // so the target store is visible before it dereferences it.
    s.target.store (target, std::memory_order_relaxed);
    s.needsReset.store (true, std::memory_order_relaxed);
    s.active.store (true, std::memory_order_release);

    return true;
}

void ModulationEngine::clearModifier (int slot) noexcept
{
    if (! isValidSlot (slot))
        return;

    // The audio thread restores the parameter the first block it sees this go
    // inactive, so the restore cannot race a write already in flight.
    slots[(size_t) slot].active.store (false, std::memory_order_release);
}

ModulationEngine::ModifierInfo ModulationEngine::getModifier (int slot) const
{
    ModifierInfo info;

    if (! isValidSlot (slot))
        return info;

    const auto& s = slots[(size_t) slot];

    info.active = s.active.load (std::memory_order_acquire);
    info.effectId = s.effectId;
    info.parameterId = s.parameterId;
    info.source = s.source.load (std::memory_order_relaxed);
    info.low = s.low.load (std::memory_order_relaxed);
    info.high = s.high.load (std::memory_order_relaxed);
    info.rateHz = s.rateHz.load (std::memory_order_relaxed);

    return info;
}

bool ModulationEngine::getBaseValue (const std::string& effectId,
                                     const std::string& parameterId,
                                     float& out) const
{
    for (const auto& s : slots)
    {
        if (s.active.load (std::memory_order_acquire) && s.effectId == effectId && s.parameterId == parameterId)
        {
            out = s.restoreValue.load (std::memory_order_relaxed);
            return true;
        }
    }

    return false;
}

void ModulationEngine::process (float inputLevel, int numSamples) noexcept
{
    if (numSamples <= 0)
        return;

    const double blockSeconds = (double) numSamples / sampleRate;

    for (auto& s : slots)
    {
        const bool nowActive = s.active.load (std::memory_order_acquire);

        if (! nowActive)
        {
            if (s.wasActive)
            {
                // Just switched off: put the parameter back where it was.
                if (const auto* target = s.target.load (std::memory_order_acquire))
                    if (target->set)
                        target->set (juce::jlimit (target->minValue, target->maxValue,
                                                   s.restoreValue.load (std::memory_order_relaxed)));

                s.wasActive = false;
            }

            continue;
        }

        const auto* target = s.target.load (std::memory_order_acquire);

        if (target == nullptr || ! target->set)
            continue;

        if (s.needsReset.exchange (false, std::memory_order_relaxed))
        {
            s.phase = 0.0;
            s.env = 0.0f;
        }

        const auto source = (Source) s.source.load (std::memory_order_relaxed);

        float value01;

        if (source == Source::envelope)
        {
            // One-pole follower on the block's input level, block-rate. The time
            // constant is in real time, so it behaves the same at any block size.
            const float coeff = 1.0f - (float) std::exp (-blockSeconds / 0.02);
            s.env += (juce::jmax (0.0f, inputLevel) - s.env) * coeff;

            // A little sensitivity so ordinary picking spans the sweep.
            value01 = juce::jlimit (0.0f, 1.0f, s.env * 3.3f);
        }
        else
        {
            const double increment = (double) s.rateHz.load (std::memory_order_relaxed) * blockSeconds;
            s.phase += increment;
            s.phase -= std::floor (s.phase);

            value01 = lfoValue (source, s.phase);
        }

        const float low = s.low.load (std::memory_order_relaxed);
        const float high = s.high.load (std::memory_order_relaxed);
        const float effective = juce::jlimit (target->minValue, target->maxValue,
                                              low + value01 * (high - low));

        target->set (effective);
        s.wasActive = true;
    }
}
} // namespace milodikfx::dsp
