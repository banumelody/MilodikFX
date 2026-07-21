#include "dsp/EQProcessor.h"

namespace milodikfx::dsp
{
namespace
{
constexpr double kBassFreqHz = 120.0;
constexpr double kMidFreqHz = 1000.0;
constexpr double kTrebleFreqHz = 7000.0;

constexpr double kMidQ = 0.9;

constexpr float kSmoothingSeconds = 0.02f;

float clampDb (float db) noexcept
{
    return juce::jlimit (-12.0f, 12.0f, db);
}
} // namespace

void EQProcessor::snapToTargets()
{
    bass.smoothed.snapTo (bassDb.load (std::memory_order_relaxed));
    mid.smoothed.snapTo (midDb.load (std::memory_order_relaxed));
    treble.smoothed.snapTo (trebleDb.load (std::memory_order_relaxed));

    // Force a rebuild: fresh filters would otherwise stay at their unit-gain
    // default while the "nothing changed" guard suppressed the first update.
    bass.builtForDb = std::numeric_limits<float>::quiet_NaN();
    mid.builtForDb = std::numeric_limits<float>::quiet_NaN();
    treble.builtForDb = std::numeric_limits<float>::quiet_NaN();
}

void EQProcessor::prepareToPlay (double sampleRate, int, int numChannels)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    currentNumChannels = juce::jlimit (0, kMaxChannels, numChannels);

    // Smoothing is stepped once per sample, so the rate is the sample rate.
    bass.smoothed.reset (currentSampleRate, kSmoothingSeconds, 0.0f);
    mid.smoothed.reset (currentSampleRate, kSmoothingSeconds, 0.0f);
    treble.smoothed.reset (currentSampleRate, kSmoothingSeconds, 0.0f);

    snapToTargets();
    reset();

    prepared = true;
}

void EQProcessor::reset()
{
    for (auto& f : bass.states) f.reset();
    for (auto& f : mid.states) f.reset();
    for (auto& f : treble.states) f.reset();
}

void EQProcessor::setBassDb (float db) noexcept
{
    bassDb.store (clampDb (db), std::memory_order_relaxed);
}

float EQProcessor::getBassDb() const noexcept
{
    return bassDb.load (std::memory_order_relaxed);
}

void EQProcessor::setMidDb (float db) noexcept
{
    midDb.store (clampDb (db), std::memory_order_relaxed);
}

float EQProcessor::getMidDb() const noexcept
{
    return midDb.load (std::memory_order_relaxed);
}

void EQProcessor::setTrebleDb (float db) noexcept
{
    trebleDb.store (clampDb (db), std::memory_order_relaxed);
}

float EQProcessor::getTrebleDb() const noexcept
{
    return trebleDb.load (std::memory_order_relaxed);
}

void EQProcessor::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable, std::memory_order_relaxed);
}

bool EQProcessor::isEnabled() const noexcept
{
    return enabled.load (std::memory_order_relaxed);
}

void EQProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (! prepared || ! enabled.load (std::memory_order_relaxed))
        return;

    const auto numSamples = buffer.getNumSamples();
    const auto numCh = juce::jmin (buffer.getNumChannels(), currentNumChannels, kMaxChannels);

    if (numSamples <= 0 || numCh <= 0)
        return;

    const auto bassTarget = bassDb.load (std::memory_order_relaxed);
    const auto midTarget = midDb.load (std::memory_order_relaxed);
    const auto trebleTarget = trebleDb.load (std::memory_order_relaxed);

    auto* const* channels = buffer.getArrayOfWritePointers();

    for (int i = 0; i < numSamples; ++i)
    {
        const auto b = bass.smoothed.next (bassTarget);
        const auto m = mid.smoothed.next (midTarget);
        const auto t = treble.smoothed.next (trebleTarget);

        if (! (std::abs (b - bass.builtForDb) < kRecomputeThresholdDb))
        {
            bass.builtForDb = b;
            bass.coeffs = biquad::makeLowShelf (currentSampleRate, kBassFreqHz, b);
        }

        if (! (std::abs (m - mid.builtForDb) < kRecomputeThresholdDb))
        {
            mid.builtForDb = m;
            mid.coeffs = biquad::makePeak (currentSampleRate, kMidFreqHz, kMidQ, m);
        }

        if (! (std::abs (t - treble.builtForDb) < kRecomputeThresholdDb))
        {
            treble.builtForDb = t;
            treble.coeffs = biquad::makeHighShelf (currentSampleRate, kTrebleFreqHz, t);
        }

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto s = channels[ch][i];
            s = bass.states[(size_t) ch].process (bass.coeffs, s);
            s = mid.states[(size_t) ch].process (mid.coeffs, s);
            s = treble.states[(size_t) ch].process (treble.coeffs, s);
            channels[ch][i] = s;
        }
    }
}
} // namespace milodikfx::dsp
