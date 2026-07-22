#include "dsp/OverdriveProcessor.h"

#include <cmath>

namespace milodikfx::dsp
{
OverdriveProcessor::OverdriveProcessor() = default;
OverdriveProcessor::~OverdriveProcessor() = default;

void OverdriveProcessor::prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels)
{
    const auto rate = sampleRate > 0.0 ? sampleRate : 44100.0;

    preparedChannels = juce::jmax (1, numChannels);
    preparedBlockSize = juce::jmax (1, samplesPerBlock);
    preparedRate = rate;

    // Force a rebuild: the voicing filters are rate-dependent.
    builtForRate = 0.0;

    for (auto& state : voiceStates)
    {
        state.reset();
        state.dc.prepare (rate * 8.0);
    }

    // Build every factor now. Selecting one later is then just an index read,
    // with no chance of allocating while audio is running.
    for (int i = 0; i < kNumOversamplers; ++i)
    {
        oversamplers[(size_t) i] = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) preparedChannels,
            (size_t) (i + 1), // factor exponent: 1 -> 2x, 2 -> 4x, 3 -> 8x
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true,
            false);

        oversamplers[(size_t) i]->initProcessing ((size_t) preparedBlockSize);
        oversamplers[(size_t) i]->reset();
    }

    // The drive smoother is stepped in the oversampled domain. Use the highest
    // factor as the reference so the glide never becomes audibly slower when a
    // lower factor is selected; the difference is imperceptible either way.
    smoothedDrive.reset (rate * 8.0, 0.02, driveAmount.load (std::memory_order_relaxed));
    smoothedAsymmetry.reset (rate * 8.0, 0.02, asymmetry.load (std::memory_order_relaxed));
    smoothedLevel.reset (rate, 0.02, levelLinear.load (std::memory_order_relaxed));

    prepared = true;
}

juce::dsp::Oversampling<float>* OverdriveProcessor::activeOversampler() const noexcept
{
    if (! oversamplingEnabled.load (std::memory_order_relaxed))
        return nullptr;

    const auto index = oversamplingIndex.load (std::memory_order_relaxed);

    if (index <= 0 || index > kNumOversamplers)
        return nullptr;

    return oversamplers[(size_t) (index - 1)].get();
}

float OverdriveProcessor::softClip (float x) noexcept
{
    if (std::abs (x) <= 1.0f)
        return x - (x * x * x) / 3.0f;

    return x > 0.0f ? (2.0f / 3.0f) : (-2.0f / 3.0f);
}

void OverdriveProcessor::applyDrive (float* const* channels,
                                     int numChannels,
                                     int numSamples,
                                     float driveTarget,
                                     float asymmetryTarget) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        const auto amount = smoothedDrive.next (driveTarget);
        const auto asym = smoothedAsymmetry.next (asymmetryTarget);

        // Pre-gain curve: 1x..20x, quadratic so the low end of the knob is usable.
        const auto driveGain = 1.0f + (amount * amount * 19.0f);

        // Bias the signal into the curve, then subtract what that bias alone
        // would have produced. Without the second term the offset would leak
        // through as DC.
        const auto bias = asym * kMaxBias;
        const auto biasOutput = softClip (bias);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto x = channels[ch][i];

            // Cubic soft clip, scaled so saturation lands at +/-1.0.
            const auto clipped = 1.5f * (softClip (x * driveGain + bias) - biasOutput);

            channels[ch][i] = ((1.0f - amount) * x) + (amount * clipped);
        }
    }
}

void OverdriveProcessor::updateVoicingIfNeeded (double rate) noexcept
{
    const auto t = type.load (std::memory_order_relaxed);
    const auto tone = tonePercent.load (std::memory_order_relaxed);
    const auto voice = voicePercent.load (std::memory_order_relaxed);
    const auto bass = bassDb.load (std::memory_order_relaxed);
    const auto mid = midDb.load (std::memory_order_relaxed);
    const auto treble = trebleDb.load (std::memory_order_relaxed);
    const auto hp = highPeakMode.load (std::memory_order_relaxed);
    const auto isBright = bright.load (std::memory_order_relaxed);

    if (t == builtForType && tone == builtForTone && voice == builtForVoice
        && bass == builtForBass && mid == builtForMid && treble == builtForTreble
        && hp == builtForHp && isBright == builtForBright && rate == builtForRate)
        return;

    builtForType = t;
    builtForTone = tone;
    builtForVoice = voice;
    builtForBass = bass;
    builtForMid = mid;
    builtForTreble = treble;
    builtForHp = hp;
    builtForBright = isBright;
    builtForRate = rate;

    const auto& v = drive::voicingFor (t);

    // Where the low end leaves the clipper's path. Dumble's Voice control moves
    // this corner, which is what makes it go from thick to nasal; OCD's HP mode
    // drops it so far more low end is driven.
    auto splitHz = v.splitHz;

    if (t == drive::dumble)
        splitHz = 200.0f + (voice / 100.0f) * 700.0f;
    else if (t == drive::ocd && hp)
        splitHz = 90.0f;
    else if (t == drive::transparent)
        // The Klon's Bass control is a pre-clip low cut rather than a shelf.
        splitHz = juce::jlimit (20.0f, 400.0f, 40.0f - bass * 12.0f);

    // Two real filters rather than "subtract the low-passed copy". The
    // subtraction looked equivalent and was not: at a corner well above the
    // note, the low-passed copy is nearly the whole signal but phase-shifted, so
    // the remainder was a sizeable phase-difference term that then got the
    // clipper's full 40x. A Tube Screamer measured as distorting bass *harder*
    // than a full-range drive, which is precisely backwards. The circuit uses
    // separate networks for the two paths, and so does this.
    splitCoeffs = splitHz > 0.0f ? biquad::makeLowPass (rate, splitHz, 0.707) : BiquadCoeffs {};
    splitHighCoeffs = splitHz > 0.0f ? biquad::makeHighPass (rate, splitHz, 0.707) : BiquadCoeffs {};

    // The RAT's LM308 gain network lifts mids and treble *before* the clipper,
    // so those frequencies clip while the lows stay cleaner. This is what makes
    // it a RAT rather than a harder OCD.
    preEmphasisCoeffs = v.preEmphasisDb != 0.0f && v.preEmphasisHz > 0.0f
                            ? biquad::makeHighShelf (rate, v.preEmphasisHz, v.preEmphasisDb)
                            : BiquadCoeffs {};

    // The knob position, reversed for a RAT (clockwise is darker there).
    const auto tonePos = v.toneReversed ? (1.0f - tone / 100.0f) : (tone / 100.0f);

    if (v.toneMode == ToneMode::midScoopTilt)
    {
        // Big Muff: a low-pass and a high-pass around a common pivot. The knob
        // tilts between them, and at noon both contribute and the mid scoops --
        // the notch is the whole point of a Muff. The crossfade happens in
        // applyVoicedDrive; here we just build the two legs.
        const auto pivot = v.toneMinHz > 0.0f ? v.toneMinHz : 900.0f;
        toneCoeffs = biquad::makeLowPass (rate, pivot, 0.707);
        toneHighCoeffs = biquad::makeHighPass (rate, pivot, 0.707);
    }
    else
    {
        const auto hasSweep = v.toneMaxHz > v.toneMinHz;
        const auto toneHz = hasSweep ? v.toneMinHz + tonePos * (v.toneMaxHz - v.toneMinHz) : 0.0f;

        toneCoeffs = hasSweep ? biquad::makeLowPass (rate, toneHz, 0.707) : BiquadCoeffs {};
        toneHighCoeffs = BiquadCoeffs {};
    }

    auto presenceHz = v.presenceHz;
    auto presenceDb = v.presenceDb;

    if (t == drive::cleanBoost && isBright)
    {
        // The EP booster's lift is the reason people buy them.
        presenceHz = 3000.0f;
        presenceDb = 2.5f;
    }

    presenceCoeffs = presenceDb != 0.0f && presenceHz > 0.0f
                         ? biquad::makeHighShelf (rate, presenceHz, presenceDb)
                         : BiquadCoeffs {};

    // Marshall-in-a-box gets a real three-band stack after the clipper; the
    // transparent voicing only uses the treble cut.
    if (t == drive::marshallInABox)
    {
        bassCoeffs = biquad::makeLowShelf (rate, 100.0f, bass);
        midCoeffs = biquad::makePeak (rate, 650.0f, 0.8, mid);
        trebleCoeffs = biquad::makeHighShelf (rate, 3000.0f, treble);
    }
    else if (t == drive::transparent)
    {
        bassCoeffs = BiquadCoeffs {};
        midCoeffs = BiquadCoeffs {};
        trebleCoeffs = biquad::makeHighShelf (rate, 2500.0f, treble);
    }
    else
    {
        bassCoeffs = BiquadCoeffs {};
        midCoeffs = BiquadCoeffs {};
        trebleCoeffs = BiquadCoeffs {};
    }
}

void OverdriveProcessor::applyVoicedDrive (float* const* channels,
                                           int numChannels,
                                           int numSamples,
                                           double rate,
                                           float driveTarget) noexcept
{
    updateVoicingIfNeeded (rate);

    const auto t = type.load (std::memory_order_relaxed);
    const auto& v = drive::voicingFor (t);

    const auto usable = juce::jmin (numChannels, kMaxVoiceChannels);

    auto driveMax = v.driveMax;

    if (t == drive::ocd && highPeakMode.load (std::memory_order_relaxed))
        driveMax *= 1.4f;

    const auto splitting = v.splitHz > 0.0f || t == drive::transparent;
    const auto tiltTone = v.toneMode == ToneMode::midScoopTilt;
    const auto hasTone = v.toneMaxHz > v.toneMinHz || tiltTone;
    const auto hasPreEmphasis = v.preEmphasisDb != 0.0f && v.preEmphasisHz > 0.0f;
    const auto hasPresence = presenceCoeffs.b0 != 1.0f || presenceCoeffs.b1 != 0.0f;
    const auto stack = t == drive::marshallInABox;
    const auto hasTreble = stack || t == drive::transparent;

    // The tilt crossfade amount, forward regardless of toneReversed (the Muff's
    // tone is not reversed, but keep the mapping explicit).
    const auto tonePctVal = tonePercent.load (std::memory_order_relaxed);
    const auto tilt = v.toneReversed ? (1.0f - tonePctVal / 100.0f) : (tonePctVal / 100.0f);

    const auto outputGain = juce::Decibels::decibelsToGain (v.outputDb);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto amount = smoothedDrive.next (driveTarget);
        const auto asym = smoothedAsymmetry.next (asymmetry.load (std::memory_order_relaxed));

        // Quadratic so the bottom of the knob stays usable across a range that
        // runs from a clean boost's 3x to an OCD's 60x.
        const auto driveGain = 1.0f + amount * amount * (driveMax - 1.0f);

        const auto stageGain = v.stages > 1 ? std::sqrt (driveGain) : driveGain;

        const auto bias = juce::jlimit (0.0f, 1.0f, v.bias + asym * kMaxBias);
        const auto biasOutput = applyCurve (v.curve, bias);

        // The clean blend rises with gain, which is what keeps a transparent
        // drive sounding like the guitar rather than like the pedal.
        const auto blend = v.cleanBlend * amount;

        for (int ch = 0; ch < usable; ++ch)
        {
            auto& state = voiceStates[(size_t) ch];

            const auto dryIn = channels[ch][i];

            // Split: the low end goes around the clipper and is added back
            // clean. This, not an EQ curve, is where a Tube Screamer's mid-hump
            // actually comes from.
            const auto low = splitting ? state.split.process (splitCoeffs, dryIn) : 0.0f;
            auto x = splitting ? state.splitHigh.process (splitHighCoeffs, dryIn) : dryIn;

            // Pre-emphasis before the clipper: the RAT lifts mids and treble
            // going in, so they distort while the lows stay cleaner.
            if (hasPreEmphasis)
                x = state.preEmphasis.process (preEmphasisCoeffs, x);

            // Cascaded stages split the gain between them rather than each
            // taking the lot. Two full-gain stages drove everything into a
            // square wave, and once the DC blocker centred that the asymmetry
            // stopped producing any even harmonics at all -- the very thing a
            // two-stage voicing is chosen for. A real cascade is gentler per
            // stage and compounds; this matches that.
            x = applyCurve (v.curve, x * stageGain + bias) - biasOutput;

            if (v.stages > 1)
                x = applyCurve (v.curve, x * stageGain + bias) - biasOutput;

            x = state.dc.process (x);

            if (tiltTone)
            {
                // Crossfade the low-pass and high-pass legs; at noon both are
                // present and the middle scoops -- the Big Muff's voice.
                const auto lp = state.tone.process (toneCoeffs, x);
                const auto hpv = state.toneHigh.process (toneHighCoeffs, x);
                x = lp * (1.0f - tilt) + hpv * tilt;
            }
            else if (hasTone)
            {
                x = state.tone.process (toneCoeffs, x);
            }

            if (hasPresence)
                x = state.presence.process (presenceCoeffs, x);

            if (stack)
            {
                x = state.bass.process (bassCoeffs, x);
                x = state.mid.process (midCoeffs, x);
            }

            if (hasTreble)
                x = state.treble.process (trebleCoeffs, x);

            auto wet = (x + low) * outputGain;

            if (blend > 0.0f)
                wet = wet * (1.0f - blend) + dryIn * blend;

            if (! std::isfinite (wet))
                wet = 0.0f;

            // The Drive knob still crossfades to clean at the bottom, so a
            // voicing at zero drive is the signal it was given.
            channels[ch][i] = ((1.0f - amount) * dryIn) + (amount * wet);
        }
    }
}

void OverdriveProcessor::applyLevel (float* const* channels, int numChannels, int numSamples, float levelTarget) noexcept
{
    if (std::abs (smoothedLevel.getCurrent() - levelTarget) < 1.0e-6f)
    {
        if (levelTarget == 1.0f)
            return;

        for (int ch = 0; ch < numChannels; ++ch)
            juce::FloatVectorOperations::multiply (channels[ch], levelTarget, numSamples);

        return;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const auto g = smoothedLevel.next (levelTarget);

        for (int ch = 0; ch < numChannels; ++ch)
            channels[ch][i] *= g;
    }
}

void OverdriveProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (! prepared)
        return;

    if (! enabled.load (std::memory_order_relaxed))
    {
        smoothedDrive.snapTo (driveAmount.load (std::memory_order_relaxed));
        smoothedAsymmetry.snapTo (asymmetry.load (std::memory_order_relaxed));
        smoothedLevel.snapTo (levelLinear.load (std::memory_order_relaxed));
        return;
    }

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    const auto driveTarget = driveAmount.load (std::memory_order_relaxed);
    const auto asymmetryTarget = asymmetry.load (std::memory_order_relaxed);
    const auto levelTarget = levelLinear.load (std::memory_order_relaxed);

    auto* const* channels = buffer.getArrayOfWritePointers();

    // No drive means no nonlinearity, so skip the oversampler entirely.
    const auto driveIsIdle = driveTarget <= 0.0f && smoothedDrive.getCurrent() <= 1.0e-6f;

    auto* oversampler = activeOversampler();

    const auto canOversample = ! driveIsIdle
                               && oversampler != nullptr
                               && numChannels == preparedChannels
                               && numSamples <= preparedBlockSize;

    if (driveIsIdle)
    {
        smoothedDrive.snapTo (driveTarget);
        smoothedAsymmetry.snapTo (asymmetryTarget);
    }
    else if (canOversample)
    {
        juce::dsp::AudioBlock<float> block (channels, (size_t) numChannels, (size_t) numSamples);
        auto upBlock = oversampler->processSamplesUp (juce::dsp::AudioBlock<const float> (block));

        const auto upSamples = (int) upBlock.getNumSamples();
        const auto upChannels = (int) upBlock.getNumChannels();

        // AudioBlock does not expose an array-of-pointers, so drive each channel
        // through a small stack array of the channel pointers it does expose.
        float* upPointers[kMaxBlockChannels];
        const auto usableChannels = juce::jmin (upChannels, kMaxBlockChannels);

        for (int ch = 0; ch < usableChannels; ++ch)
            upPointers[ch] = upBlock.getChannelPointer ((size_t) ch);

        // The voicing filters run inside the drive path, so their coefficients
        // have to be built for the oversampled rate, not the device rate.
        const auto upRate = preparedRate * (double) (upSamples / juce::jmax (1, numSamples));

        if (drive::isVoiced (type.load (std::memory_order_relaxed)))
            applyVoicedDrive (upPointers, usableChannels, upSamples, upRate, driveTarget);
        else
            applyDrive (upPointers, usableChannels, upSamples, driveTarget, asymmetryTarget);

        oversampler->processSamplesDown (block);
    }
    else if (drive::isVoiced (type.load (std::memory_order_relaxed)))
    {
        applyVoicedDrive (channels, numChannels, numSamples, preparedRate, driveTarget);
    }
    else
    {
        applyDrive (channels, numChannels, numSamples, driveTarget, asymmetryTarget);
    }

    applyLevel (channels, numChannels, numSamples, levelTarget);
}

void OverdriveProcessor::reset()
{
    for (auto& os : oversamplers)
        if (os != nullptr)
            os->reset();

    smoothedDrive.snapTo (driveAmount.load (std::memory_order_relaxed));
    smoothedAsymmetry.snapTo (asymmetry.load (std::memory_order_relaxed));
    smoothedLevel.snapTo (levelLinear.load (std::memory_order_relaxed));

    for (auto& state : voiceStates)
        state.reset();
}

void OverdriveProcessor::setType (int newType) noexcept
{
    type.store (juce::jlimit (0, drive::numTypes - 1, newType), std::memory_order_relaxed);
}

int OverdriveProcessor::getType() const noexcept
{
    return type.load (std::memory_order_relaxed);
}

void OverdriveProcessor::setTonePercent (float percent) noexcept
{
    tonePercent.store (juce::jlimit (0.0f, 100.0f, percent), std::memory_order_relaxed);
}

float OverdriveProcessor::getTonePercent() const noexcept
{
    return tonePercent.load (std::memory_order_relaxed);
}

void OverdriveProcessor::setVoicePercent (float percent) noexcept
{
    voicePercent.store (juce::jlimit (0.0f, 100.0f, percent), std::memory_order_relaxed);
}

float OverdriveProcessor::getVoicePercent() const noexcept
{
    return voicePercent.load (std::memory_order_relaxed);
}

void OverdriveProcessor::setBassDb (float db) noexcept
{
    bassDb.store (juce::jlimit (-12.0f, 12.0f, db), std::memory_order_relaxed);
}

float OverdriveProcessor::getBassDb() const noexcept
{
    return bassDb.load (std::memory_order_relaxed);
}

void OverdriveProcessor::setMidDb (float db) noexcept
{
    midDb.store (juce::jlimit (-12.0f, 12.0f, db), std::memory_order_relaxed);
}

float OverdriveProcessor::getMidDb() const noexcept
{
    return midDb.load (std::memory_order_relaxed);
}

void OverdriveProcessor::setTrebleDb (float db) noexcept
{
    trebleDb.store (juce::jlimit (-12.0f, 12.0f, db), std::memory_order_relaxed);
}

float OverdriveProcessor::getTrebleDb() const noexcept
{
    return trebleDb.load (std::memory_order_relaxed);
}

void OverdriveProcessor::setHighPeakMode (bool shouldUseHp) noexcept
{
    highPeakMode.store (shouldUseHp, std::memory_order_relaxed);
}

bool OverdriveProcessor::isHighPeakMode() const noexcept
{
    return highPeakMode.load (std::memory_order_relaxed);
}

void OverdriveProcessor::setBright (bool shouldBrighten) noexcept
{
    bright.store (shouldBrighten, std::memory_order_relaxed);
}

bool OverdriveProcessor::isBright() const noexcept
{
    return bright.load (std::memory_order_relaxed);
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

void OverdriveProcessor::setOversamplingEnabled (bool shouldEnable) noexcept
{
    oversamplingEnabled.store (shouldEnable, std::memory_order_relaxed);
}

bool OverdriveProcessor::isOversamplingEnabled() const noexcept
{
    return oversamplingEnabled.load (std::memory_order_relaxed);
}

void OverdriveProcessor::setOversamplingIndex (int index) noexcept
{
    oversamplingIndex.store (juce::jlimit (0, kNumOversamplers, index), std::memory_order_relaxed);
}

int OverdriveProcessor::getOversamplingIndex() const noexcept
{
    return oversamplingIndex.load (std::memory_order_relaxed);
}

void OverdriveProcessor::setAsymmetry (float amount) noexcept
{
    asymmetry.store (juce::jlimit (0.0f, 1.0f, amount), std::memory_order_relaxed);
}

float OverdriveProcessor::getAsymmetry() const noexcept
{
    return asymmetry.load (std::memory_order_relaxed);
}

float OverdriveProcessor::getLatencySamples() const noexcept
{
    if (auto* os = activeOversampler())
        return os->getLatencyInSamples();

    return 0.0f;
}
} // namespace milodikfx::dsp
