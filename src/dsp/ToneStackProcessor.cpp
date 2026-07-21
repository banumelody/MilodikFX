#include "ToneStackProcessor.h"

namespace milodikfx::dsp {

namespace
{
constexpr double kBassFreqHz = 50.0;
constexpr double kMidFreqHz = 500.0;
constexpr double kTrebleFreqHz = 5000.0;

constexpr double kQ = 0.7;

constexpr float kSmoothingSeconds = 0.02f;

float clampDb (float db) noexcept
{
    return juce::jlimit (-12.0f, 12.0f, db);
}
} // namespace

void ToneStackProcessor::snapToTargets()
{
    bass.smoothed.snapTo (bassDb.load (std::memory_order_relaxed));
    mid.smoothed.snapTo (midDb.load (std::memory_order_relaxed));
    treble.smoothed.snapTo (trebleDb.load (std::memory_order_relaxed));

    bass.builtForDb = std::numeric_limits<float>::quiet_NaN();
    mid.builtForDb = std::numeric_limits<float>::quiet_NaN();
    treble.builtForDb = std::numeric_limits<float>::quiet_NaN();
}

void ToneStackProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock, int numChannels)
{
    juce::ignoreUnused (samplesPerBlock);

    sampleRate = sampleRateIn > 0.0 ? sampleRateIn : 44100.0;
    currentNumChannels = juce::jlimit (0, kMaxChannels, numChannels);

    bass.smoothed.reset (sampleRate, kSmoothingSeconds, 0.0f);
    mid.smoothed.reset (sampleRate, kSmoothingSeconds, 0.0f);
    treble.smoothed.reset (sampleRate, kSmoothingSeconds, 0.0f);

    snapToTargets();
    reset();

    prepared = true;
}

void ToneStackProcessor::processBlock (juce::AudioBuffer<float>& buffer)
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
            bass.coeffs = biquad::makePeak (sampleRate, kBassFreqHz, kQ, b);
        }

        if (! (std::abs (m - mid.builtForDb) < kRecomputeThresholdDb))
        {
            mid.builtForDb = m;
            mid.coeffs = biquad::makePeak (sampleRate, kMidFreqHz, kQ, m);
        }

        if (! (std::abs (t - treble.builtForDb) < kRecomputeThresholdDb))
        {
            treble.builtForDb = t;
            treble.coeffs = biquad::makePeak (sampleRate, kTrebleFreqHz, kQ, t);
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

void ToneStackProcessor::reset()
{
    for (auto& s : bass.states) s.reset();
    for (auto& s : mid.states) s.reset();
    for (auto& s : treble.states) s.reset();
}

void ToneStackProcessor::setBassDb (float db) noexcept
{
    bassDb.store (clampDb (db), std::memory_order_relaxed);
}

void ToneStackProcessor::setMidDb (float db) noexcept
{
    midDb.store (clampDb (db), std::memory_order_relaxed);
}

void ToneStackProcessor::setTrebleDb (float db) noexcept
{
    trebleDb.store (clampDb (db), std::memory_order_relaxed);
}

void ToneStackProcessor::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable, std::memory_order_relaxed);
}

float ToneStackProcessor::getBassDb() const noexcept { return bassDb.load (std::memory_order_relaxed); }
float ToneStackProcessor::getMidDb() const noexcept { return midDb.load (std::memory_order_relaxed); }
float ToneStackProcessor::getTrebleDb() const noexcept { return trebleDb.load (std::memory_order_relaxed); }
bool ToneStackProcessor::isEnabled() const noexcept { return enabled.load (std::memory_order_relaxed); }

} // namespace milodikfx::dsp
