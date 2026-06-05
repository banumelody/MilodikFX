#include "dsp/EQProcessor.h"

namespace milodikfx::dsp
{
namespace
{
constexpr double kBassFreqHz = 120.0;
constexpr double kMidFreqHz = 1000.0;
constexpr double kTrebleFreqHz = 7000.0;

constexpr float kShelfQ = 0.707f;
constexpr float kMidQ = 0.9f;

static float clampDb (float db) noexcept
{
    return juce::jlimit (-12.0f, 12.0f, db);
}
} // namespace

void EQProcessor::prepareToPlay (double sampleRate, int, int numChannels)
{
    currentSampleRate = sampleRate;
    currentNumChannels = juce::jmax (0, numChannels);

    bassFilters.clear();
    midFilters.clear();
    trebleFilters.clear();

    bassFilters.resize ((size_t) currentNumChannels);
    midFilters.resize ((size_t) currentNumChannels);
    trebleFilters.resize ((size_t) currentNumChannels);

    reset();
    prepared = true;

    lastBassDb = bassDb.load (std::memory_order_relaxed);
    lastMidDb = midDb.load (std::memory_order_relaxed);
    lastTrebleDb = trebleDb.load (std::memory_order_relaxed);

    updateCoefficientsIfNeeded();
}

void EQProcessor::reset()
{
    for (auto& f : bassFilters)
        f.reset();
    for (auto& f : midFilters)
        f.reset();
    for (auto& f : trebleFilters)
        f.reset();
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

void EQProcessor::updateCoefficientsIfNeeded()
{
    if (currentSampleRate <= 0.0 || currentNumChannels <= 0)
        return;

    const auto b = bassDb.load (std::memory_order_relaxed);
    const auto m = midDb.load (std::memory_order_relaxed);
    const auto t = trebleDb.load (std::memory_order_relaxed);

    if (bassCoefs == nullptr || b != lastBassDb)
    {
        lastBassDb = b;
        const auto g = juce::Decibels::decibelsToGain (b);
        bassCoefs = juce::dsp::IIR::Coefficients<float>::makeLowShelf (currentSampleRate, kBassFreqHz, kShelfQ, (float) g);
        for (auto& f : bassFilters)
            f.coefficients = bassCoefs;
    }

    if (midCoefs == nullptr || m != lastMidDb)
    {
        lastMidDb = m;
        const auto g = juce::Decibels::decibelsToGain (m);
        midCoefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSampleRate, kMidFreqHz, kMidQ, (float) g);
        for (auto& f : midFilters)
            f.coefficients = midCoefs;
    }

    if (trebleCoefs == nullptr || t != lastTrebleDb)
    {
        lastTrebleDb = t;
        const auto g = juce::Decibels::decibelsToGain (t);
        trebleCoefs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, kTrebleFreqHz, kShelfQ, (float) g);
        for (auto& f : trebleFilters)
            f.coefficients = trebleCoefs;
    }
}

void EQProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (! prepared)
        return;

    if (! enabled.load (std::memory_order_relaxed))
        return;

    updateCoefficientsIfNeeded();

    const auto numCh = juce::jmin (buffer.getNumChannels(), currentNumChannels);

    juce::dsp::AudioBlock<float> block (buffer);

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto channelBlock = block.getSingleChannelBlock ((size_t) ch);
        juce::dsp::ProcessContextReplacing<float> ctx (channelBlock);

        bassFilters[(size_t) ch].process (ctx);
        midFilters[(size_t) ch].process (ctx);
        trebleFilters[(size_t) ch].process (ctx);
    }
}
} // namespace milodikfx::dsp
