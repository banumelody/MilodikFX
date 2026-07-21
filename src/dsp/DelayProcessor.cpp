#include "dsp/DelayProcessor.h"

#include <algorithm>
#include <cmath>

namespace milodikfx::dsp
{
namespace
{
// Slow on purpose: this is what turns a time change into a tape-style glide
// instead of a click.
constexpr float kTimeSmoothingSeconds = 0.15f;
constexpr float kLevelSmoothingSeconds = 0.02f;
} // namespace

DelayProcessor::DelayProcessor()
{
    // Allocated once and never touched again, so a device change cannot pull
    // this memory out from under a block that is already being processed.
    lines.assign ((size_t) kMaxChannels, std::vector<float> ((size_t) kLineLength, 0.0f));
}

void DelayProcessor::prepareToPlay (double sampleRateIn, int, int)
{
    sampleRate = sampleRateIn > 0.0 ? sampleRateIn : 44100.0;

    // The delay time in samples depends on the rate, but the storage does not.
    const auto delaySamples = juce::jlimit (1.0f,
                                            (float) (kLineLength - 2),
                                            getEffectiveTimeMs() * 0.001f * (float) sampleRate);

    smoothedDelaySamples.reset (sampleRate, kTimeSmoothingSeconds, delaySamples);
    smoothedFeedback.reset (sampleRate, kLevelSmoothingSeconds, feedbackPercent.load (std::memory_order_relaxed) / 100.0f);
    smoothedMix.reset (sampleRate, kLevelSmoothingSeconds, mixPercent.load (std::memory_order_relaxed) / 100.0f);

    reset();

    prepared = true;
}

void DelayProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (! prepared || ! enabled.load (std::memory_order_relaxed))
        return;

    const auto numSamples = buffer.getNumSamples();
    const auto numCh = juce::jmin (buffer.getNumChannels(), kMaxChannels);

    if (numSamples <= 0 || numCh <= 0)
        return;

    if (writeIndex < 0 || writeIndex >= kLineLength)
        writeIndex = 0;

    const auto delayTarget = juce::jlimit (1.0f,
                                           (float) (kLineLength - 2),
                                           getEffectiveTimeMs() * 0.001f * (float) sampleRate);
    const auto feedbackTarget = juce::jlimit (0.0f, 0.95f, feedbackPercent.load (std::memory_order_relaxed) / 100.0f);
    const auto mixTarget = juce::jlimit (0.0f, 1.0f, mixPercent.load (std::memory_order_relaxed) / 100.0f);

    const auto damping = juce::jlimit (500.0f, 20000.0f, dampingHz.load (std::memory_order_relaxed));
    const auto dampingActive = damping < kDampingOffHz;

    if (dampingActive && std::abs (damping - builtDampingForHz) > 1.0f)
    {
        builtDampingForHz = damping;
        dampingCoeffs = biquad::makeLowPass (sampleRate, damping, 0.707);
    }

    const auto crossFeed = pingPong.load (std::memory_order_relaxed) && numCh > 1;

    auto* const* channels = buffer.getArrayOfWritePointers();

    // Delayed value per channel for this sample, read before anything is written
    // so ping-pong can cross the feedback without reading a half-updated line.
    float delayed[kMaxChannels] {};

    for (int i = 0; i < numSamples; ++i)
    {
        const auto delaySamples = juce::jlimit (1.0f, (float) (kLineLength - 2),
                                                smoothedDelaySamples.next (delayTarget));
        const auto feedback = smoothedFeedback.next (feedbackTarget);
        const auto mix = smoothedMix.next (mixTarget);

        auto readPos = (float) writeIndex - delaySamples;

        while (readPos < 0.0f)
            readPos += (float) kLineLength;

        auto readIndex = (int) readPos;
        const auto frac = readPos - (float) readIndex;

        readIndex = juce::jlimit (0, kLineLength - 1, readIndex);
        const auto nextIndex = readIndex + 1 >= kLineLength ? 0 : readIndex + 1;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const auto& line = lines[(size_t) ch];
            delayed[ch] = line[(size_t) readIndex] * (1.0f - frac) + line[(size_t) nextIndex] * frac;
        }

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto& line = lines[(size_t) ch];

            // Ping-pong takes its feedback from the other channel, so each repeat
            // lands on the opposite side.
            const auto feedbackSource = crossFeed ? delayed[ch == 0 ? 1 : 0] : delayed[ch];

            auto toFeedBack = feedbackSource * feedback;

            if (dampingActive)
                toFeedBack = dampingStates[(size_t) ch].process (dampingCoeffs, toFeedBack);

            const auto input = channels[ch][i];

            auto written = input + toFeedBack;

            if (! std::isfinite (written))
                written = 0.0f;

            line[(size_t) writeIndex] = juce::jlimit (-4.0f, 4.0f, written);

            channels[ch][i] = input * (1.0f - mix) + delayed[ch] * mix;
        }

        if (++writeIndex >= kLineLength)
            writeIndex = 0;
    }
}

void DelayProcessor::reset()
{
    for (auto& line : lines)
        std::fill (line.begin(), line.end(), 0.0f);

    for (auto& state : dampingStates)
        state.reset();

    writeIndex = 0;
}

void DelayProcessor::setDampingHz (float hz) noexcept
{
    dampingHz.store (juce::jlimit (500.0f, 20000.0f, hz), std::memory_order_relaxed);
}

float DelayProcessor::getDampingHz() const noexcept
{
    return dampingHz.load (std::memory_order_relaxed);
}

void DelayProcessor::setPingPong (bool shouldPingPong) noexcept
{
    pingPong.store (shouldPingPong, std::memory_order_relaxed);
}

bool DelayProcessor::isPingPong() const noexcept
{
    return pingPong.load (std::memory_order_relaxed);
}

void DelayProcessor::setSyncDivision (int index) noexcept
{
    syncDivision.store (juce::jlimit (0, kNumSyncDivisions - 1, index), std::memory_order_relaxed);
}

int DelayProcessor::getSyncDivision() const noexcept
{
    return syncDivision.load (std::memory_order_relaxed);
}

void DelayProcessor::setBpm (float beatsPerMinute) noexcept
{
    bpm.store (juce::jlimit (30.0f, 300.0f, beatsPerMinute), std::memory_order_relaxed);
}

float DelayProcessor::getBpm() const noexcept
{
    return bpm.load (std::memory_order_relaxed);
}

float DelayProcessor::getEffectiveTimeMs() const noexcept
{
    const auto division = (SyncDivision) syncDivision.load (std::memory_order_relaxed);

    if (division == SyncDivision::off)
        return timeMs.load (std::memory_order_relaxed);

    const auto beats = juce::jlimit (30.0f, 300.0f, bpm.load (std::memory_order_relaxed));
    const auto quarterMs = 60000.0f / beats;

    auto ms = quarterMs;

    switch (division)
    {
        case SyncDivision::quarter:       ms = quarterMs; break;
        case SyncDivision::eighthDotted:  ms = quarterMs * 0.75f; break;
        case SyncDivision::eighth:        ms = quarterMs * 0.5f; break;
        case SyncDivision::eighthTriplet: ms = quarterMs / 3.0f; break;
        case SyncDivision::sixteenth:     ms = quarterMs * 0.25f; break;
        case SyncDivision::off:           break;
    }

    // A quarter note at 30 bpm is 2 s, past what the line holds, so the same
    // bounds the manual control has apply to the synced time too.
    return juce::jlimit (10.0f, kMaxDelayMs, ms);
}

void DelayProcessor::setTimeMs (float ms) noexcept
{
    timeMs.store (juce::jlimit (10.0f, kMaxDelayMs, ms), std::memory_order_relaxed);
}

float DelayProcessor::getTimeMs() const noexcept
{
    return timeMs.load (std::memory_order_relaxed);
}

void DelayProcessor::setFeedbackPercent (float percent) noexcept
{
    feedbackPercent.store (juce::jlimit (0.0f, 95.0f, percent), std::memory_order_relaxed);
}

float DelayProcessor::getFeedbackPercent() const noexcept
{
    return feedbackPercent.load (std::memory_order_relaxed);
}

void DelayProcessor::setMixPercent (float percent) noexcept
{
    mixPercent.store (juce::jlimit (0.0f, 100.0f, percent), std::memory_order_relaxed);
}

float DelayProcessor::getMixPercent() const noexcept
{
    return mixPercent.load (std::memory_order_relaxed);
}

void DelayProcessor::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable, std::memory_order_relaxed);
}

bool DelayProcessor::isEnabled() const noexcept
{
    return enabled.load (std::memory_order_relaxed);
}
} // namespace milodikfx::dsp
