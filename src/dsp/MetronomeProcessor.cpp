#include "dsp/MetronomeProcessor.h"

#include <cmath>

namespace milodikfx::dsp
{
void MetronomeProcessor::prepareToPlay (double sampleRateIn, int, int)
{
    sampleRate = sampleRateIn > 0.0 ? sampleRateIn : 44100.0;

    // One-pole decay reaching about -60 dB over the click length.
    clickDecay = (float) std::exp (-1.0 / (kClickDecaySeconds * sampleRate) * 6.9);

    reset();

    prepared = true;
}

void MetronomeProcessor::reset()
{
    samplesUntilBeat = 0.0;
    nextBeatInBar = 0;
    clickPhase = 0.0f;
    clickEnvelope = 0.0f;
    beatCount.store (0, std::memory_order_relaxed);
    beatInBar.store (0, std::memory_order_relaxed);
}

void MetronomeProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (! prepared)
        return;

    if (! enabled.load (std::memory_order_relaxed))
    {
        // Drop the schedule so switching on always lands the first click
        // immediately rather than part-way into a stale bar.
        if (samplesUntilBeat != 0.0 || clickEnvelope != 0.0f)
        {
            samplesUntilBeat = 0.0;
            nextBeatInBar = 0;
            clickEnvelope = 0.0f;
        }

        return;
    }

    const auto numSamples = buffer.getNumSamples();
    const auto numCh = buffer.getNumChannels();

    if (numSamples <= 0 || numCh <= 0)
        return;

    const auto beats = juce::jlimit (kMinBpm, kMaxBpm, bpm.load (std::memory_order_relaxed));
    const auto samplesPerBeat = sampleRate * 60.0 / (double) beats;
    const auto bar = juce::jlimit (1, kMaxBeatsPerBar, beatsPerBar.load (std::memory_order_relaxed));
    const auto level = juce::jlimit (0.0f, 1.0f, volumePercent.load (std::memory_order_relaxed) / 100.0f) * kClickPeak;

    auto* const* channels = buffer.getArrayOfWritePointers();

    for (int i = 0; i < numSamples; ++i)
    {
        if (samplesUntilBeat <= 0.0)
        {
            const auto accented = bar > 1 && nextBeatInBar == 0;

            clickPhase = 0.0f;
            clickIncrement = juce::MathConstants<float>::twoPi
                             * (accented ? kAccentHz : kClickHz) / (float) sampleRate;
            clickEnvelope = 1.0f;

            beatInBar.store (nextBeatInBar, std::memory_order_relaxed);
            beatCount.fetch_add (1, std::memory_order_relaxed);

            if (++nextBeatInBar >= bar)
                nextBeatInBar = 0;

            samplesUntilBeat += samplesPerBeat;
        }

        samplesUntilBeat -= 1.0;

        if (clickEnvelope > 1.0e-4f)
        {
            const auto click = std::sin (clickPhase) * clickEnvelope * level;

            clickPhase += clickIncrement;

            if (clickPhase > juce::MathConstants<float>::twoPi)
                clickPhase -= juce::MathConstants<float>::twoPi;

            clickEnvelope *= clickDecay;

            for (int ch = 0; ch < numCh; ++ch)
            {
                // Its own clamp: this runs after the limiter, so nothing
                // downstream would catch a sum that went over full scale.
                auto sum = channels[ch][i] + click;

                if (! std::isfinite (sum))
                    sum = 0.0f;

                channels[ch][i] = juce::jlimit (-1.0f, 1.0f, sum);
            }
        }
        else
        {
            clickEnvelope = 0.0f;
        }
    }
}

void MetronomeProcessor::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable, std::memory_order_relaxed);
}

bool MetronomeProcessor::isEnabled() const noexcept
{
    return enabled.load (std::memory_order_relaxed);
}

void MetronomeProcessor::setBpm (float beatsPerMinute) noexcept
{
    bpm.store (juce::jlimit (kMinBpm, kMaxBpm, beatsPerMinute), std::memory_order_relaxed);
}

float MetronomeProcessor::getBpm() const noexcept
{
    return bpm.load (std::memory_order_relaxed);
}

void MetronomeProcessor::setVolumePercent (float percent) noexcept
{
    volumePercent.store (juce::jlimit (0.0f, 100.0f, percent), std::memory_order_relaxed);
}

float MetronomeProcessor::getVolumePercent() const noexcept
{
    return volumePercent.load (std::memory_order_relaxed);
}

void MetronomeProcessor::setBeatsPerBar (int beats) noexcept
{
    beatsPerBar.store (juce::jlimit (1, kMaxBeatsPerBar, beats), std::memory_order_relaxed);
}

int MetronomeProcessor::getBeatsPerBar() const noexcept
{
    return beatsPerBar.load (std::memory_order_relaxed);
}

int MetronomeProcessor::getBeatCount() const noexcept
{
    return beatCount.load (std::memory_order_relaxed);
}

int MetronomeProcessor::getBeatInBar() const noexcept
{
    return beatInBar.load (std::memory_order_relaxed);
}
} // namespace milodikfx::dsp
