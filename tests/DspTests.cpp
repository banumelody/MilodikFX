#include <JuceHeader.h>

#include <cmath>
#include <limits>

#include "dsp/CabinetProcessor.h"
#include "dsp/CompressorProcessor.h"
#include "dsp/DelayProcessor.h"
#include "dsp/EQProcessor.h"
#include "dsp/MasterOutProcessor.h"
#include "dsp/NoiseGateProcessor.h"
#include "dsp/OverdriveProcessor.h"
#include "dsp/ReverbProcessor.h"
#include "dsp/ToneStackProcessor.h"

namespace
{
constexpr double kRate = 48000.0;

void fillSine (juce::AudioBuffer<float>& buffer, double sampleRate, double freqHz, float amplitude, int startSample = 0)
{
    const auto twoPi = juce::MathConstants<double>::twoPi;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto phase = twoPi * freqHz * (double) (i + startSample) / sampleRate;
        const auto s = (float) (amplitude * std::sin (phase));

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample (ch, i, s);
    }
}

float rms (const juce::AudioBuffer<float>& buffer, int startSample = 0, int numSamples = -1)
{
    const auto count = numSamples < 0 ? buffer.getNumSamples() - startSample : numSamples;

    if (count <= 0 || buffer.getNumChannels() <= 0)
        return 0.0f;

    double sumSq = 0.0;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < count; ++i)
        {
            const auto s = (double) buffer.getSample (ch, startSample + i);
            sumSq += s * s;
        }

    return (float) std::sqrt (sumSq / (double) (count * buffer.getNumChannels()));
}

float peak (const juce::AudioBuffer<float>& buffer)
{
    return buffer.getMagnitude (0, buffer.getNumSamples());
}

bool allFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (buffer.getSample (ch, i)))
                return false;

    return true;
}
} // namespace

//==============================================================================
class CompressorProcessorTests final : public juce::UnitTest
{
public:
    CompressorProcessorTests() : juce::UnitTest ("CompressorProcessor") {}

    void runTest() override
    {
        beginTest ("Hot signal at default settings never produces a non-finite sample");

        // Regression: the old gain computer divided by the (negative) threshold,
        // so any peak past roughly -16 dBFS produced Inf, then log10 of a
        // negative number, and the NaN latched into the envelope for good.
        milodikfx::dsp::CompressorProcessor comp;
        comp.setEnabled (true);
        comp.prepareToPlay (kRate, 512, 1);

        expect (std::abs (comp.getThresholdDb() - (-24.0f)) < 0.001f);
        expect (std::abs (comp.getRatio() - 4.0f) < 0.001f);

        juce::AudioBuffer<float> buffer (1, 512);

        for (int block = 0; block < 8; ++block)
        {
            fillSine (buffer, kRate, 220.0, 0.9f, block * 512);
            comp.processBlock (buffer);
            expect (allFinite (buffer), "block " + juce::String (block) + " contained a non-finite sample");
        }

        beginTest ("Signal keeps flowing after a hot block (no latched envelope)");

        fillSine (buffer, kRate, 220.0, 0.05f);
        comp.processBlock (buffer);

        expect (allFinite (buffer));
        expect (rms (buffer) > 0.001f, "output went silent after the loud passage");

        beginTest ("Louder input gets proportionally more gain reduction");

        milodikfx::dsp::CompressorProcessor c2;
        c2.setEnabled (true);
        c2.setAutoMakeupGain (false);
        c2.setThresholdDb (-24.0f);
        c2.setRatio (4.0f);
        c2.setAttackMs (0.5f);
        c2.setReleaseMs (10.0f);
        c2.prepareToPlay (kRate, 8192, 1);

        const auto quietRatio = measureGainRatio (c2, 0.02f);
        c2.reset();
        const auto loudRatio = measureGainRatio (c2, 0.9f);

        logMessage ("quiet in/out ratio " + juce::String (quietRatio) + ", loud " + juce::String (loudRatio));
        expect (loudRatio < quietRatio * 0.7f, "loud signal was not compressed relative to the quiet one");
        expect (loudRatio < 1.0f);

        beginTest ("Auto makeup gain is not an exact bypass");

        // Regression: makeup used to be the exact reciprocal of the envelope,
        // so the compressor multiplied by g and then by 1/g.
        milodikfx::dsp::CompressorProcessor c3;
        c3.setEnabled (true);
        c3.setAutoMakeupGain (true);
        c3.setThresholdDb (-24.0f);
        c3.setRatio (4.0f);
        c3.setAttackMs (0.5f);
        c3.setReleaseMs (10.0f);
        c3.prepareToPlay (kRate, 8192, 1);

        juce::AudioBuffer<float> makeupBuf (1, 8192);
        fillSine (makeupBuf, kRate, 220.0, 0.5f);
        const auto before = rms (makeupBuf, 4096);

        c3.processBlock (makeupBuf);
        const auto after = rms (makeupBuf, 4096);

        logMessage ("makeup rms before " + juce::String (before) + " after " + juce::String (after));
        expect (std::abs (after - before) > before * 0.05f, "compressor behaved as a bypass");
        expect (c3.getGainReductionDb() < 0.0f, "no gain reduction was reported");

        beginTest ("Parallel mix at 0% leaves the signal untouched");

        milodikfx::dsp::CompressorProcessor mixOff;
        mixOff.setEnabled (true);
        mixOff.setThresholdDb (-40.0f);
        mixOff.setRatio (20.0f);
        mixOff.setInputGainDb (12.0f);
        mixOff.setMixPercent (0.0f);
        mixOff.prepareToPlay (kRate, 2048, 1);

        juce::AudioBuffer<float> mixBuf (1, 2048);
        fillSine (mixBuf, kRate, 220.0, 0.5f);

        juce::AudioBuffer<float> mixOriginal (1, 2048);
        mixOriginal.makeCopyOf (mixBuf);

        mixOff.processBlock (mixBuf);

        for (int i = 0; i < mixBuf.getNumSamples(); ++i)
            expect (std::abs (mixBuf.getSample (0, i) - mixOriginal.getSample (0, i)) < 1.0e-5f,
                    "mix 0% altered the signal at sample " + juce::String (i));

        beginTest ("Parallel mix sits between dry and fully compressed");

        const auto fullyWet = compressWith (100.0f);
        const auto halfWet = compressWith (50.0f);
        const auto fullyDry = compressWith (0.0f);

        logMessage ("rms dry " + juce::String (fullyDry)
                    + " half " + juce::String (halfWet)
                    + " wet " + juce::String (fullyWet));

        // Heavy compression with no makeup pulls the level down, so the blend
        // has to land between the two extremes.
        expect (fullyWet < fullyDry, "compression did not reduce the level");
        expect (halfWet > fullyWet && halfWet < fullyDry, "mix 50% did not land between dry and wet");

        beginTest ("Disabled is passthrough");

        milodikfx::dsp::CompressorProcessor c4;
        c4.setEnabled (false);
        c4.prepareToPlay (kRate, 256, 1);

        juce::AudioBuffer<float> passBuf (1, 256);
        fillSine (passBuf, kRate, 440.0, 0.3f);

        juce::AudioBuffer<float> copy (1, 256);
        copy.makeCopyOf (passBuf);

        c4.processBlock (passBuf);

        for (int i = 0; i < passBuf.getNumSamples(); ++i)
            expect (std::abs (passBuf.getSample (0, i) - copy.getSample (0, i)) < 1.0e-6f);
    }

private:
    /** RMS of a heavily compressed sine at a given parallel-mix setting. */
    static float compressWith (float mixPercent)
    {
        milodikfx::dsp::CompressorProcessor comp;
        comp.setEnabled (true);
        comp.setAutoMakeupGain (false);
        comp.setThresholdDb (-40.0f);
        comp.setRatio (20.0f);
        comp.setAttackMs (0.5f);
        comp.setReleaseMs (10.0f);
        comp.setMixPercent (mixPercent);
        comp.prepareToPlay (kRate, 8192, 1);

        juce::AudioBuffer<float> buffer (1, 8192);
        fillSine (buffer, kRate, 220.0, 0.5f);

        comp.processBlock (buffer);

        return rms (buffer, 4096);
    }

    static float measureGainRatio (milodikfx::dsp::CompressorProcessor& comp, float amplitude)
    {
        juce::AudioBuffer<float> buffer (1, 8192);
        fillSine (buffer, kRate, 220.0, amplitude);

        const auto in = rms (buffer, 4096);
        comp.processBlock (buffer);
        const auto out = rms (buffer, 4096);

        return in > 0.0f ? out / in : 0.0f;
    }
};

static CompressorProcessorTests compressorProcessorTests;

//==============================================================================
class OverdriveShapeTests final : public juce::UnitTest
{
public:
    OverdriveShapeTests() : juce::UnitTest ("OverdriveProcessor shaping") {}

    void runTest() override
    {
        beginTest ("Asymmetry 0 clips both polarities identically");

        // Oversampling off so the measurement is of the curve itself rather than
        // of the half-band filter's ringing.
        const auto symmetric = measurePolarityDifference (0.0f);
        logMessage ("polarity difference at asymmetry 0: " + juce::String (symmetric));
        expect (symmetric < 0.001f, "symmetric curve was not symmetric");

        beginTest ("Asymmetry biases one polarity more than the other");

        const auto asymmetric = measurePolarityDifference (1.0f);
        logMessage ("polarity difference at asymmetry 1: " + juce::String (asymmetric));
        expect (asymmetric > 0.02f, "asymmetry had no effect on the curve");

        beginTest ("Asymmetry introduces no DC on a silent input");

        milodikfx::dsp::OverdriveProcessor od;
        od.setEnabled (true);
        od.setDrivePercent (100.0f);
        od.setAsymmetry (1.0f);
        od.setOversamplingIndex (0);
        od.prepareToPlay (kRate, 1024, 1);

        juce::AudioBuffer<float> silent (1, 1024);
        silent.clear();
        od.processBlock (silent);

        for (int i = 0; i < silent.getNumSamples(); ++i)
            expect (std::abs (silent.getSample (0, i)) < 1.0e-5f,
                    "bias leaked through as DC: " + juce::String (silent.getSample (0, i)));

        beginTest ("Every oversampling factor produces usable audio");

        for (int index = 0; index <= 3; ++index)
        {
            milodikfx::dsp::OverdriveProcessor osTest;
            osTest.setEnabled (true);
            osTest.setDrivePercent (80.0f);
            osTest.setOversamplingIndex (index);
            osTest.prepareToPlay (kRate, 1024, 2);

            juce::AudioBuffer<float> buffer (2, 1024);

            for (int block = 0; block < 4; ++block)
            {
                fillSine (buffer, kRate, 440.0, 0.4f, block * 1024);
                osTest.processBlock (buffer);

                expect (allFinite (buffer), "factor index " + juce::String (index) + " produced a non-finite sample");
                expect (peak (buffer) <= 2.0f, "factor index " + juce::String (index) + " ran away");
            }

            expect (rms (buffer) > 0.01f, "factor index " + juce::String (index) + " produced silence");

            const auto latency = osTest.getLatencySamples();
            logMessage ("oversampling index " + juce::String (index)
                        + " latency " + juce::String (latency) + " samples");

            if (index == 0)
                expect (latency == 0.0f, "no oversampling should mean no added latency");
        }
    }

private:
    /** How differently the curve treats a positive and a negative input. */
    static float measurePolarityDifference (float asymmetry)
    {
        auto measure = [asymmetry] (float level)
        {
            milodikfx::dsp::OverdriveProcessor od;
            od.setEnabled (true);
            od.setDrivePercent (100.0f);
            od.setLevelPercent (100.0f);
            od.setAsymmetry (asymmetry);
            od.setOversamplingIndex (0);
            od.prepareToPlay (kRate, 512, 1);

            juce::AudioBuffer<float> buffer (1, 512);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (0, i, level);

            od.processBlock (buffer);

            return buffer.getSample (0, buffer.getNumSamples() - 1);
        };

        return std::abs (std::abs (measure (0.6f)) - std::abs (measure (-0.6f)));
    }
};

static OverdriveShapeTests overdriveShapeTests;

//==============================================================================
class ReverbProcessorTests final : public juce::UnitTest
{
public:
    ReverbProcessorTests() : juce::UnitTest ("ReverbProcessor") {}

    void runTest() override
    {
        beginTest ("Fully wet output stays in the same league as the dry input");

        // Regression: the freeverb input attenuation was missing, which made the
        // wet path roughly 20 dB hotter than dry.
        milodikfx::dsp::ReverbProcessor rev;
        rev.setEnabled (true);
        rev.setDryWetMix (1.0f);
        rev.setRoomSize (0.5f);
        rev.setDecayTime (2.0f);
        rev.prepareToPlay (kRate, 4096, 2);

        juce::AudioBuffer<float> buffer (2, 4096);

        float wetRms = 0.0f;
        float dryRms = 0.0f;

        // Let the tail build up over several blocks before measuring.
        for (int block = 0; block < 12; ++block)
        {
            fillSine (buffer, kRate, 220.0, 0.3f, block * 4096);
            dryRms = rms (buffer);
            rev.processBlock (buffer);
            wetRms = rms (buffer);
            expect (allFinite (buffer));
        }

        const auto ratioDb = juce::Decibels::gainToDecibels (wetRms / juce::jmax (1.0e-9f, dryRms));
        logMessage ("wet/dry ratio: " + juce::String (ratioDb) + " dB");

        expect (ratioDb < 6.0f, "wet path is far too hot: " + juce::String (ratioDb) + " dB");
        expect (ratioDb > -30.0f, "wet path is inaudible: " + juce::String (ratioDb) + " dB");

        beginTest ("Decay time changes the tail length");

        const auto shortTail = measureTailEnergy (0.3f);
        const auto longTail = measureTailEnergy (8.0f);

        logMessage ("tail energy short " + juce::String (shortTail) + " long " + juce::String (longTail));
        expect (longTail > shortTail * 2.0f, "decay time knob had no effect on the tail");

        beginTest ("Width control changes the stereo difference");

        // Regression: identical left/right comb tunings made width a no-op.
        const auto narrow = measureStereoDifference (0.0f);
        const auto wide = measureStereoDifference (1.0f);

        logMessage ("L/R difference narrow " + juce::String (narrow) + " wide " + juce::String (wide));
        expect (wide > narrow * 1.5f, "width knob had no effect");

        beginTest ("Dry/wet at 0 leaves the signal untouched");

        milodikfx::dsp::ReverbProcessor dryOnly;
        dryOnly.setEnabled (true);
        dryOnly.setDryWetMix (0.0f);
        dryOnly.prepareToPlay (kRate, 1024, 2);

        juce::AudioBuffer<float> dryBuf (2, 1024);
        fillSine (dryBuf, kRate, 440.0, 0.25f);

        juce::AudioBuffer<float> copy (2, 1024);
        copy.makeCopyOf (dryBuf);

        dryOnly.processBlock (dryBuf);

        for (int i = 0; i < dryBuf.getNumSamples(); ++i)
            expect (std::abs (dryBuf.getSample (0, i) - copy.getSample (0, i)) < 1.0e-5f);
    }

private:
    static float measureTailEnergy (float decaySeconds)
    {
        milodikfx::dsp::ReverbProcessor rev;
        rev.setEnabled (true);
        rev.setDryWetMix (1.0f);
        rev.setRoomSize (0.5f);
        rev.setDecayTime (decaySeconds);
        rev.prepareToPlay (kRate, 4096, 2);

        juce::AudioBuffer<float> buffer (2, 4096);

        // Excite with a burst...
        for (int block = 0; block < 4; ++block)
        {
            fillSine (buffer, kRate, 220.0, 0.5f, block * 4096);
            rev.processBlock (buffer);
        }

        // ...then measure what is left after a second of silence.
        float tail = 0.0f;

        for (int block = 0; block < 12; ++block)
        {
            buffer.clear();
            rev.processBlock (buffer);
            tail = rms (buffer);
        }

        return tail;
    }

    static float measureStereoDifference (float width)
    {
        milodikfx::dsp::ReverbProcessor rev;
        rev.setEnabled (true);
        rev.setDryWetMix (1.0f);
        rev.setRoomSize (0.5f);
        rev.setDecayTime (3.0f);
        rev.setWidth (width);
        rev.prepareToPlay (kRate, 4096, 2);

        juce::AudioBuffer<float> buffer (2, 4096);

        double sumSq = 0.0;
        int count = 0;

        for (int block = 0; block < 8; ++block)
        {
            fillSine (buffer, kRate, 220.0, 0.3f, block * 4096);
            rev.processBlock (buffer);

            if (block >= 4)
            {
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                {
                    const auto d = (double) buffer.getSample (0, i) - (double) buffer.getSample (1, i);
                    sumSq += d * d;
                    ++count;
                }
            }
        }

        return count > 0 ? (float) std::sqrt (sumSq / (double) count) : 0.0f;
    }
};

static ReverbProcessorTests reverbProcessorTests;

//==============================================================================
class EQRestartTests final : public juce::UnitTest
{
public:
    EQRestartTests() : juce::UnitTest ("EQProcessor restart") {}

    void runTest() override
    {
        beginTest ("EQ still boosts after a second prepareToPlay");

        // Regression: prepareToPlay rebuilt the filters and then synced the
        // "last value" cache before the coefficient update ran, so the guard saw
        // no change and the fresh filters never received any coefficients --
        // the EQ silently went flat after every device restart.
        milodikfx::dsp::EQProcessor eq;
        eq.setEnabled (true);
        eq.setMidDb (12.0f);

        eq.prepareToPlay (kRate, 4096, 1);
        const auto firstBoost = measureMidBoost (eq);

        eq.prepareToPlay (kRate, 4096, 1);
        const auto secondBoost = measureMidBoost (eq);

        logMessage ("boost before restart " + juce::String (firstBoost) + ", after " + juce::String (secondBoost));

        expect (firstBoost > 2.5f, "EQ did not boost before the restart");
        expect (secondBoost > 2.5f, "EQ went flat after the restart");
        expect (std::abs (secondBoost - firstBoost) < firstBoost * 0.1f);

        beginTest ("Sample-rate change rebuilds coefficients");

        milodikfx::dsp::EQProcessor eq2;
        eq2.setEnabled (true);
        eq2.setMidDb (12.0f);

        eq2.prepareToPlay (44100.0, 4096, 1);
        expect (measureMidBoost (eq2, 44100.0) > 2.5f);

        eq2.prepareToPlay (96000.0, 4096, 1);
        expect (measureMidBoost (eq2, 96000.0) > 2.5f);

        beginTest ("Changing a band mid-stream does not produce a discontinuity");

        milodikfx::dsp::EQProcessor eq3;
        eq3.setEnabled (true);
        eq3.setBassDb (0.0f);
        eq3.prepareToPlay (kRate, 512, 1);

        juce::AudioBuffer<float> buffer (1, 512);
        fillSine (buffer, kRate, 220.0, 0.3f);
        eq3.processBlock (buffer);

        const auto lastBefore = buffer.getSample (0, buffer.getNumSamples() - 1);

        eq3.setBassDb (12.0f);
        fillSine (buffer, kRate, 220.0, 0.3f, 512);
        eq3.processBlock (buffer);

        const auto firstAfter = buffer.getSample (0, 0);

        // A step change in coefficients would show up as a jump far larger than
        // one sample of a 220 Hz sine at this amplitude.
        expect (std::abs (firstAfter - lastBefore) < 0.1f,
                "bass change stepped instead of gliding: " + juce::String (std::abs (firstAfter - lastBefore)));
    }

private:
    static float measureMidBoost (milodikfx::dsp::EQProcessor& eq, double rate = kRate)
    {
        juce::AudioBuffer<float> buffer (1, 4096);
        fillSine (buffer, rate, 1000.0, 0.2f);

        const auto before = rms (buffer);
        eq.processBlock (buffer);
        const auto after = rms (buffer);

        return before > 0.0f ? after / before : 0.0f;
    }
};

static EQRestartTests eqRestartTests;

//==============================================================================
class NoiseGateProcessorTests final : public juce::UnitTest
{
public:
    NoiseGateProcessorTests() : juce::UnitTest ("NoiseGateProcessor") {}

    void runTest() override
    {
        beginTest ("Disabled is passthrough");

        milodikfx::dsp::NoiseGateProcessor gate;
        gate.setEnabled (false);
        gate.prepareToPlay (kRate, 512, 1);

        juce::AudioBuffer<float> buffer (1, 512);
        fillSine (buffer, kRate, 440.0, 0.001f);

        juce::AudioBuffer<float> copy (1, 512);
        copy.makeCopyOf (buffer);

        gate.processBlock (buffer);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            expect (std::abs (buffer.getSample (0, i) - copy.getSample (0, i)) < 1.0e-9f);

        beginTest ("Closes on a signal below the threshold");

        milodikfx::dsp::NoiseGateProcessor g2;
        g2.setEnabled (true);
        g2.setThresholdDb (-40.0f);
        g2.setHoldMs (0.0f);
        g2.setReleaseMs (10.0f);
        g2.prepareToPlay (kRate, 4096, 1);

        juce::AudioBuffer<float> quiet (1, 4096);

        for (int block = 0; block < 8; ++block)
        {
            fillSine (quiet, kRate, 440.0, 0.001f, block * 4096); // about -60 dBFS
            g2.processBlock (quiet);
        }

        expect (rms (quiet) < 1.0e-5f, "gate did not close on a quiet signal");
        expect (g2.getCurrentGain() < 0.1f);

        beginTest ("Opens on a signal above the threshold");

        juce::AudioBuffer<float> loud (1, 4096);

        for (int block = 0; block < 4; ++block)
        {
            fillSine (loud, kRate, 440.0, 0.3f, block * 4096);
            g2.processBlock (loud);
        }

        expect (rms (loud) > 0.15f, "gate stayed shut on a loud signal");
        expect (g2.getCurrentGain() > 0.9f);
        expect (allFinite (loud));
    }
};

static NoiseGateProcessorTests noiseGateProcessorTests;

//==============================================================================
class CabinetProcessorTests final : public juce::UnitTest
{
public:
    CabinetProcessorTests() : juce::UnitTest ("CabinetProcessor") {}

    void runTest() override
    {
        beginTest ("Rolls off fizz well above the speaker corner");

        const auto at1k = measureResponse (1000.0);
        const auto at10k = measureResponse (10000.0);
        const auto at30Hz = measureResponse (30.0);

        logMessage ("cab response 30 Hz " + juce::String (at30Hz)
                    + ", 1 kHz " + juce::String (at1k)
                    + ", 10 kHz " + juce::String (at10k));

        expect (at10k < at1k * 0.2f, "10 kHz was not attenuated relative to 1 kHz");
        expect (at30Hz < at1k * 0.5f, "sub-bass was not rolled off");
        expect (at1k > 0.3f, "the midrange was crushed");

        beginTest ("Disabled is passthrough");

        milodikfx::dsp::CabinetProcessor cab;
        cab.setEnabled (false);
        cab.prepareToPlay (kRate, 512, 1);

        juce::AudioBuffer<float> buffer (1, 512);
        fillSine (buffer, kRate, 1000.0, 0.3f);

        juce::AudioBuffer<float> copy (1, 512);
        copy.makeCopyOf (buffer);

        cab.processBlock (buffer);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            expect (std::abs (buffer.getSample (0, i) - copy.getSample (0, i)) < 1.0e-9f);
    }

private:
    static float measureResponse (double freqHz)
    {
        milodikfx::dsp::CabinetProcessor cab;
        cab.setEnabled (true);
        cab.prepareToPlay (kRate, 8192, 1);

        juce::AudioBuffer<float> buffer (1, 8192);

        // Run one block to settle the filter state, then measure the next.
        fillSine (buffer, kRate, freqHz, 0.3f);
        cab.processBlock (buffer);

        fillSine (buffer, kRate, freqHz, 0.3f, 8192);
        const auto before = rms (buffer);
        cab.processBlock (buffer);
        const auto after = rms (buffer);

        return before > 0.0f ? after / before : 0.0f;
    }
};

static CabinetProcessorTests cabinetProcessorTests;

//==============================================================================
class DelayProcessorTests final : public juce::UnitTest
{
public:
    DelayProcessorTests() : juce::UnitTest ("DelayProcessor") {}

    void runTest() override
    {
        beginTest ("Disabled is passthrough");

        milodikfx::dsp::DelayProcessor delay;
        delay.setEnabled (false);
        delay.prepareToPlay (kRate, 512, 1);

        juce::AudioBuffer<float> buffer (1, 512);
        fillSine (buffer, kRate, 440.0, 0.3f);

        juce::AudioBuffer<float> copy (1, 512);
        copy.makeCopyOf (buffer);

        delay.processBlock (buffer);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            expect (std::abs (buffer.getSample (0, i) - copy.getSample (0, i)) < 1.0e-9f);

        beginTest ("An impulse comes back one delay time later");

        milodikfx::dsp::DelayProcessor d2;
        d2.setEnabled (true);
        d2.setTimeMs (100.0f);
        d2.setFeedbackPercent (0.0f);
        d2.setMixPercent (100.0f);
        d2.prepareToPlay (kRate, 16384, 1);

        const auto delaySamples = (int) (0.1 * kRate);

        juce::AudioBuffer<float> impulse (1, 16384);
        impulse.clear();
        impulse.setSample (0, 0, 1.0f);

        d2.processBlock (impulse);

        auto loudestIndex = 0;
        auto loudest = 0.0f;

        for (int i = 1; i < impulse.getNumSamples(); ++i)
        {
            const auto v = std::abs (impulse.getSample (0, i));

            if (v > loudest)
            {
                loudest = v;
                loudestIndex = i;
            }
        }

        logMessage ("echo peaked at sample " + juce::String (loudestIndex)
                    + " (expected around " + juce::String (delaySamples) + ")");

        expect (loudest > 0.5f, "no echo came back");
        expect (std::abs (loudestIndex - delaySamples) < 64,
                "echo arrived at the wrong time: " + juce::String (loudestIndex));

        beginTest ("Damping makes later repeats darker");

        // A bright impulse train through a damped feedback loop must lose high
        // frequency with every pass. Compare total repeat energy with the filter
        // effectively off against a hard 800 Hz roll-off.
        const auto brightTail = measureRepeatEnergy (20000.0f);
        const auto darkTail = measureRepeatEnergy (800.0f);

        logMessage ("repeat energy bright " + juce::String (brightTail)
                    + " damped " + juce::String (darkTail));

        expect (darkTail < brightTail * 0.9f, "damping did not attenuate the repeats");

        beginTest ("Ping-pong moves repeats to the other channel");

        // Input on the left only. Without ping-pong the right line is fed
        // nothing, so any energy on the right proves the feedback crossed over.
        const auto normalRight = measureRightChannelEnergy (false);
        const auto pingPongRight = measureRightChannelEnergy (true);

        logMessage ("right-channel energy normal " + juce::String (normalRight)
                    + " ping-pong " + juce::String (pingPongRight));

        expect (normalRight < 1.0e-6f, "right channel had energy without ping-pong");
        expect (pingPongRight > 0.001f, "ping-pong did not reach the right channel");

        beginTest ("Survives a device change while running");

        // Regression: a "vector subscript out of range" fired from
        // DelayProcessor::processBlock during a device switch with audio
        // flowing. Every prepareToPlay resizes the delay lines, so any index
        // derived from the old size has to be invalidated with them.
        milodikfx::dsp::DelayProcessor d4;
        d4.setEnabled (true);
        d4.setTimeMs (1000.0f); // the longest line, i.e. the largest indices
        d4.setFeedbackPercent (50.0f);
        d4.setMixPercent (50.0f);

        const double rates[] = { 96000.0, 48000.0, 44100.0, 192000.0, 48000.0 };

        for (const auto rate : rates)
        {
            d4.prepareToPlay (rate, 512, 2);

            juce::AudioBuffer<float> block (2, 512);

            for (int b = 0; b < 40; ++b)
            {
                fillSine (block, rate, 220.0, 0.4f, b * 512);
                d4.processBlock (block);
                expect (allFinite (block));
            }

            // Change the time mid-stream too: that moves the read position
            // while the smoother is still gliding.
            d4.setTimeMs (10.0f);

            for (int b = 0; b < 40; ++b)
            {
                fillSine (block, rate, 220.0, 0.4f, b * 512);
                d4.processBlock (block);
                expect (allFinite (block));
            }

            d4.setTimeMs (1000.0f);
        }

        beginTest ("Feedback stays bounded");

        milodikfx::dsp::DelayProcessor d3;
        d3.setEnabled (true);
        d3.setTimeMs (50.0f);
        d3.setFeedbackPercent (95.0f);
        d3.setMixPercent (50.0f);
        d3.prepareToPlay (kRate, 8192, 1);

        juce::AudioBuffer<float> feedbackBuf (1, 8192);

        for (int block = 0; block < 40; ++block)
        {
            if (block == 0)
                fillSine (feedbackBuf, kRate, 440.0, 0.5f);
            else
                feedbackBuf.clear();

            d3.processBlock (feedbackBuf);
            expect (allFinite (feedbackBuf));
            expect (peak (feedbackBuf) < 8.0f, "delay feedback ran away at block " + juce::String (block));
        }
    }

private:
    /** Energy left in the repeats after the input stops, at a given damping. */
    static float measureRepeatEnergy (float dampingHz)
    {
        milodikfx::dsp::DelayProcessor delay;
        delay.setEnabled (true);
        delay.setTimeMs (50.0f);
        delay.setFeedbackPercent (80.0f);
        delay.setMixPercent (100.0f);
        delay.setDampingHz (dampingHz);
        delay.prepareToPlay (kRate, 4096, 1);

        juce::AudioBuffer<float> buffer (1, 4096);

        // One block of bright content, then silence while the repeats decay.
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample (0, i, (i % 8) < 4 ? 0.5f : -0.5f); // square-ish, rich in highs

        delay.processBlock (buffer);

        double energy = 0.0;

        for (int block = 0; block < 8; ++block)
        {
            buffer.clear();
            delay.processBlock (buffer);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const auto s = (double) buffer.getSample (0, i);
                energy += s * s;
            }
        }

        return (float) energy;
    }

    /** Energy that reaches the right channel when only the left is fed. */
    static float measureRightChannelEnergy (bool pingPong)
    {
        milodikfx::dsp::DelayProcessor delay;
        delay.setEnabled (true);
        delay.setTimeMs (20.0f);
        delay.setFeedbackPercent (70.0f);
        delay.setMixPercent (100.0f);
        delay.setPingPong (pingPong);
        delay.prepareToPlay (kRate, 4096, 2);

        juce::AudioBuffer<float> buffer (2, 4096);

        double energy = 0.0;

        for (int block = 0; block < 6; ++block)
        {
            buffer.clear();

            if (block == 0)
                for (int i = 0; i < 256; ++i)
                    buffer.setSample (0, i, 0.6f); // left only

            delay.processBlock (buffer);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const auto s = (double) buffer.getSample (1, i);
                energy += s * s;
            }
        }

        return (float) energy;
    }
};

static DelayProcessorTests delayProcessorTests;

//==============================================================================
class MasterOutProcessorTests final : public juce::UnitTest
{
public:
    MasterOutProcessorTests() : juce::UnitTest ("MasterOutProcessor") {}

    void runTest() override
    {
        beginTest ("Master volume can attenuate");

        // Regression: the only "master volume" in the app was a 0..+24 dB
        // pre-drive boost, so there was no way to turn anything down.
        milodikfx::dsp::MasterOutProcessor master;
        master.setVolumeDb (-12.0f);
        master.setLimiterEnabled (false);
        master.prepareToPlay (kRate, 8192, 1);

        juce::AudioBuffer<float> buffer (1, 8192);
        fillSine (buffer, kRate, 440.0, 0.2f);

        const auto before = rms (buffer, 4096);
        master.processBlock (buffer);
        const auto after = rms (buffer, 4096);

        const auto appliedDb = juce::Decibels::gainToDecibels (after / juce::jmax (1.0e-9f, before));
        logMessage ("applied " + juce::String (appliedDb) + " dB");
        expect (std::abs (appliedDb - (-12.0f)) < 0.5f);

        beginTest ("Limiter holds the output under the ceiling");

        milodikfx::dsp::MasterOutProcessor limited;
        limited.setVolumeDb (12.0f);
        limited.setLimiterEnabled (true);
        limited.setCeilingDb (-0.3f);
        limited.prepareToPlay (kRate, 4096, 2);

        const auto ceilingLinear = juce::Decibels::decibelsToGain (-0.3f);

        juce::AudioBuffer<float> hot (2, 4096);

        for (int block = 0; block < 8; ++block)
        {
            fillSine (hot, kRate, 220.0, 0.95f, block * 4096);
            limited.processBlock (hot);

            expect (allFinite (hot));
            expect (peak (hot) <= ceilingLinear + 1.0e-4f,
                    "output exceeded the ceiling: " + juce::String (peak (hot)));
        }

        expect (limited.getLimiterReductionDb() < 0.0f, "limiter reported no reduction on a hot signal");

        beginTest ("Mute silences the output");

        milodikfx::dsp::MasterOutProcessor muted;
        muted.setVolumeDb (0.0f);
        muted.setMuted (true);
        muted.prepareToPlay (kRate, 8192, 1);

        juce::AudioBuffer<float> muteBuf (1, 8192);
        fillSine (muteBuf, kRate, 440.0, 0.5f);
        muted.processBlock (muteBuf);

        expect (rms (muteBuf, 4096) < 1.0e-5f, "mute did not silence the signal");

        beginTest ("A non-finite input never reaches the output");

        milodikfx::dsp::MasterOutProcessor guard;
        guard.setVolumeDb (0.0f);
        guard.prepareToPlay (kRate, 256, 1);

        juce::AudioBuffer<float> nanBuf (1, 256);
        fillSine (nanBuf, kRate, 440.0, 0.2f);
        nanBuf.setSample (0, 10, std::numeric_limits<float>::quiet_NaN());
        nanBuf.setSample (0, 11, std::numeric_limits<float>::infinity());

        guard.processBlock (nanBuf);

        expect (allFinite (nanBuf), "NaN/Inf leaked through the master stage");
    }
};

static MasterOutProcessorTests masterOutProcessorTests;

//==============================================================================
class ToneStackProcessorTests final : public juce::UnitTest
{
public:
    ToneStackProcessorTests() : juce::UnitTest ("ToneStackProcessor") {}

    void runTest() override
    {
        beginTest ("Disabled is passthrough");

        milodikfx::dsp::ToneStackProcessor tone;
        tone.setEnabled (false);
        tone.setMidDb (12.0f);
        tone.prepareToPlay (kRate, 512, 1);

        juce::AudioBuffer<float> buffer (1, 512);
        fillSine (buffer, kRate, 500.0, 0.3f);

        juce::AudioBuffer<float> copy (1, 512);
        copy.makeCopyOf (buffer);

        tone.processBlock (buffer);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            expect (std::abs (buffer.getSample (0, i) - copy.getSample (0, i)) < 1.0e-9f);

        beginTest ("Mid band boosts 500 Hz and survives a restart");

        milodikfx::dsp::ToneStackProcessor t2;
        t2.setEnabled (true);
        t2.setMidDb (12.0f);
        t2.prepareToPlay (kRate, 8192, 1);

        const auto first = measureBoost (t2, 500.0);

        t2.prepareToPlay (kRate, 8192, 1);
        const auto second = measureBoost (t2, 500.0);

        logMessage ("tone stack boost " + juce::String (first) + " then " + juce::String (second));
        expect (first > 2.0f);
        expect (second > 2.0f, "tone stack went flat after a restart");

        beginTest ("More channels than prepared does not read out of bounds");

        milodikfx::dsp::ToneStackProcessor t3;
        t3.setEnabled (true);
        t3.setMidDb (6.0f);
        t3.prepareToPlay (kRate, 512, 1);

        juce::AudioBuffer<float> wide (4, 512);
        fillSine (wide, kRate, 500.0, 0.2f);

        t3.processBlock (wide);
        expect (allFinite (wide));
    }

private:
    static float measureBoost (milodikfx::dsp::ToneStackProcessor& tone, double freqHz)
    {
        juce::AudioBuffer<float> buffer (1, 8192);
        fillSine (buffer, kRate, freqHz, 0.2f);

        const auto before = rms (buffer);
        tone.processBlock (buffer);
        const auto after = rms (buffer);

        return before > 0.0f ? after / before : 0.0f;
    }
};

static ToneStackProcessorTests toneStackProcessorTests;
