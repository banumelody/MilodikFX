#include "dsp/CabinetProcessor.h"

namespace milodikfx::dsp
{
namespace
{
// Speaker low-frequency corner: nothing useful comes out of a 12" below this.
constexpr double kLowCutHz = 80.0;
constexpr double kLowCutQ = 0.707;

// The "thump" a closed-back cab adds just above its corner.
constexpr double kThumpHz = 105.0;
constexpr double kThumpQ = 1.0;
constexpr float kThumpDb = 3.0f;

// The lower-mid scoop that keeps a driven tone from sounding boxy.
constexpr double kBoxHz = 400.0;
constexpr double kBoxQ = 1.2;
constexpr float kBoxDb = -4.0f;

// Presence bump, offset by the user's presence control.
constexpr double kPresenceHz = 2600.0;
constexpr double kPresenceQ = 1.5;
constexpr float kPresenceBaseDb = 2.0f;

constexpr double kToneQ = 0.707;

constexpr float kSmoothingSeconds = 0.03f;
constexpr float kPresenceThresholdDb = 0.01f;
constexpr float kToneThresholdHz = 1.0f;
} // namespace

void CabinetProcessor::rebuildCoefficients (float presence, float tone) noexcept
{
    builtForPresenceDb = presence;
    builtForToneHz = tone;

    coeffs[0] = biquad::makeHighPass (sampleRate, kLowCutHz, kLowCutQ);
    coeffs[1] = biquad::makePeak (sampleRate, kThumpHz, kThumpQ, kThumpDb);
    coeffs[2] = biquad::makePeak (sampleRate, kBoxHz, kBoxQ, kBoxDb);
    coeffs[3] = biquad::makePeak (sampleRate, kPresenceHz, kPresenceQ, kPresenceBaseDb + presence);
    coeffs[4] = biquad::makeLowPass (sampleRate, tone, kToneQ);
    coeffs[5] = biquad::makeLowPass (sampleRate, tone, kToneQ);
}

void CabinetProcessor::prepareToPlay (double sampleRateIn, int samplesPerBlock, int numChannels)
{
    sampleRate = sampleRateIn > 0.0 ? sampleRateIn : 44100.0;
    currentNumChannels = juce::jlimit (0, kMaxChannels, numChannels);

    irEngine.prepare (sampleRate, juce::jmax (1, samplesPerBlock), juce::jmax (1, currentNumChannels));
    irEngineB.prepare (sampleRate, juce::jmax (1, samplesPerBlock), juce::jmax (1, currentNumChannels));

    smoothedPresence.reset (sampleRate, kSmoothingSeconds, presenceDb.load (std::memory_order_relaxed));
    smoothedTone.reset (sampleRate, kSmoothingSeconds, toneHz.load (std::memory_order_relaxed));

    rebuildCoefficients (presenceDb.load (std::memory_order_relaxed), toneHz.load (std::memory_order_relaxed));
    reset();

    prepared = true;
}

CabinetProcessor::CabinetProcessor()
{
    // Allocated once, up front. Loading a second IR mid-song must not allocate
    // under a running audio thread.
    blendScratch.setSize (kMaxChannels, kMaxIrBlock, false, true, false);
}

void CabinetProcessor::setIrBlend (float amount) noexcept
{
    irBlend.store (juce::jlimit (0.0f, 1.0f, amount), std::memory_order_relaxed);
}

float CabinetProcessor::getIrBlend() const noexcept
{
    return irBlend.load (std::memory_order_relaxed);
}

/**
 * Runs one or both impulse responses.
 *
 * Returns false when nothing usable is loaded, so the caller falls through to
 * the analytic chain rather than going silent.
 */
bool CabinetProcessor::processConvolution (juce::AudioBuffer<float>& buffer, int numSamples, int numCh)
{
    const auto hasA = irEngine.hasImpulseResponse();
    const auto hasB = irEngineB.hasImpulseResponse();

    if (! hasA && ! hasB)
        return false;

    const auto blend = irBlend.load (std::memory_order_relaxed);

    // Only one side in play: no scratch copy, no mixing.
    if (! hasB || blend <= 0.0f)
        return hasA && irEngine.process (buffer);

    if (! hasA || blend >= 1.0f)
        return irEngineB.process (buffer);

    if (numSamples > blendScratch.getNumSamples() || numCh > blendScratch.getNumChannels())
        return irEngine.process (buffer);

    for (int ch = 0; ch < numCh; ++ch)
        blendScratch.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    juce::AudioBuffer<float> bView (blendScratch.getArrayOfWritePointers(), numCh, numSamples);

    if (! irEngine.process (buffer))
        return irEngineB.process (buffer);

    if (! irEngineB.process (bView))
        return true; // A already ran; B declined, so leave A's result in place

    auto* const* a = buffer.getArrayOfWritePointers();

    for (int ch = 0; ch < numCh; ++ch)
        for (int i = 0; i < numSamples; ++i)
            a[ch][i] = a[ch][i] * (1.0f - blend) + bView.getSample (ch, i) * blend;

    return true;
}

void CabinetProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (! prepared || ! enabled.load (std::memory_order_relaxed))
        return;

    const auto numSamples = buffer.getNumSamples();
    const auto numCh = juce::jmin (buffer.getNumChannels(), currentNumChannels, kMaxChannels);

    if (numSamples <= 0 || numCh <= 0)
        return;

    // Convolution when an IR is loaded and selected; otherwise fall through to
    // the analytic chain, so a missing or unreadable file never means silence.
    if (useImpulseResponse.load (std::memory_order_relaxed) && processConvolution (buffer, numSamples, numCh))
        return;

    const auto presenceTarget = presenceDb.load (std::memory_order_relaxed);
    const auto toneTarget = toneHz.load (std::memory_order_relaxed);

    auto* const* channels = buffer.getArrayOfWritePointers();

    for (int i = 0; i < numSamples; ++i)
    {
        const auto presence = smoothedPresence.next (presenceTarget);
        const auto tone = smoothedTone.next (toneTarget);

        if (! (std::abs (presence - builtForPresenceDb) < kPresenceThresholdDb
               && std::abs (tone - builtForToneHz) < kToneThresholdHz))
            rebuildCoefficients (presence, tone);

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto s = channels[ch][i];
            auto& chainStates = states[(size_t) ch];

            for (int stage = 0; stage < kNumStages; ++stage)
                s = chainStates[(size_t) stage].process (coeffs[(size_t) stage], s);

            channels[ch][i] = s;
        }
    }
}

void CabinetProcessor::reset()
{
    for (auto& chainStates : states)
        for (auto& s : chainStates)
            s.reset();

    irEngine.reset();
    irEngineB.reset();
    blendScratch.clear();
}

void CabinetProcessor::setUseImpulseResponse (bool shouldUseIr) noexcept
{
    useImpulseResponse.store (shouldUseIr, std::memory_order_relaxed);
}

bool CabinetProcessor::isUsingImpulseResponse() const noexcept
{
    return useImpulseResponse.load (std::memory_order_relaxed);
}

void CabinetProcessor::setPresenceDb (float db) noexcept
{
    presenceDb.store (juce::jlimit (-12.0f, 12.0f, db), std::memory_order_relaxed);
}

float CabinetProcessor::getPresenceDb() const noexcept
{
    return presenceDb.load (std::memory_order_relaxed);
}

void CabinetProcessor::setToneHz (float hz) noexcept
{
    toneHz.store (juce::jlimit (2000.0f, 8000.0f, hz), std::memory_order_relaxed);
}

float CabinetProcessor::getToneHz() const noexcept
{
    return toneHz.load (std::memory_order_relaxed);
}

void CabinetProcessor::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable, std::memory_order_relaxed);
}

bool CabinetProcessor::isEnabled() const noexcept
{
    return enabled.load (std::memory_order_relaxed);
}
} // namespace milodikfx::dsp
