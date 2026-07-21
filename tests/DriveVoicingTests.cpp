#include <JuceHeader.h>

#include <cmath>
#include <vector>

#include "dsp/DriveVoicing.h"
#include "dsp/OverdriveProcessor.h"

namespace
{
constexpr double kRate = 48000.0;

/** Samples the harmonic measurements analyse. */
constexpr int kWindow = 4096;

/**
 * Test frequencies are snapped to analysis bins.
 *
 * With an arbitrary frequency the window holds a fractional number of cycles,
 * and the fundamental leaks across the whole spectrum. That leakage sat at
 * roughly -43 dB, which is the same order as the harmonics being measured --
 * so a symmetric curve appeared to have as many even harmonics as an
 * asymmetric one. On a bin the window holds a whole number of cycles and the
 * leakage disappears.
 */
constexpr double kBinHz = kRate / (double) kWindow;

constexpr double bin (int n) { return (double) n * kBinHz; }

/**
 * Magnitude at one frequency, by correlating against sin and cos.
 *
 * A whole FFT would be overkill: these tests each ask about two or three known
 * frequencies, and this says exactly what is at them.
 */
double magnitudeAt (const juce::AudioBuffer<float>& buffer, double freqHz, int start, int count)
{
    double re = 0.0;
    double im = 0.0;

    const auto* data = buffer.getReadPointer (0);

    for (int i = 0; i < count; ++i)
    {
        const auto phase = juce::MathConstants<double>::twoPi * freqHz * (double) (start + i) / kRate;
        re += data[start + i] * std::cos (phase);
        im += data[start + i] * std::sin (phase);
    }

    return 2.0 * std::sqrt (re * re + im * im) / (double) count;
}

void fillSine (juce::AudioBuffer<float>& buffer, double freqHz, float amplitude)
{
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto s = (float) (amplitude
                                * std::sin (juce::MathConstants<double>::twoPi * freqHz
                                            * (double) i / kRate));

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample (ch, i, s);
    }
}

/** Runs one voicing over a sine and returns the settled second half. */
juce::AudioBuffer<float> runVoicing (int type,
                                     double freqHz,
                                     float amplitude,
                                     float drivePct,
                                     int oversampling = 1)
{
    milodikfx::dsp::OverdriveProcessor od;
    od.setEnabled (true);
    od.setType (type);
    od.setDrivePercent (drivePct);
    od.setLevelPercent (100.0f);
    od.setOversamplingIndex (oversampling);
    od.prepareToPlay (kRate, 8192, 1);

    juce::AudioBuffer<float> buffer (1, 8192);

    // Twice, so the smoothers and the filters have settled before we measure.
    for (int pass = 0; pass < 2; ++pass)
    {
        fillSine (buffer, freqHz, amplitude);
        od.processBlock (buffer);
    }

    return buffer;
}

/** Harmonic level relative to the fundamental: level-independent. */
double harmonicRatio (const juce::AudioBuffer<float>& buffer, double fundamentalHz, double harmonicHz)
{
    const auto fundamental = magnitudeAt (buffer, fundamentalHz, kWindow, kWindow);

    if (fundamental < 1.0e-9)
        return 0.0;

    return magnitudeAt (buffer, harmonicHz, kWindow, kWindow) / fundamental;
}

float rmsOf (const juce::AudioBuffer<float>& buffer, int start, int count)
{
    double sum = 0.0;

    for (int i = 0; i < count; ++i)
    {
        const auto s = (double) buffer.getSample (0, start + i);
        sum += s * s;
    }

    return (float) std::sqrt (sum / (double) count);
}
} // namespace

//==============================================================================
class DriveVoicingTests final : public juce::UnitTest
{
public:
    DriveVoicingTests() : juce::UnitTest ("Drive voicings", "dsp") {}

    void runTest() override
    {
        using milodikfx::dsp::OverdriveProcessor;
        namespace drive = milodikfx::dsp::drive;

        beginTest ("Custom is untouched by the voicing work");
        {
            // The null test that lets old presets keep sounding the way they
            // did: type 0 must take the original code path exactly.
            expectEquals ((int) drive::custom, 0);
            expect (! drive::isVoiced (drive::custom));

            OverdriveProcessor od;
            expectEquals (od.getType(), (int) drive::custom,
                          "a fresh overdrive must default to the original voicing");
        }

        beginTest ("Every voicing produces a finite, bounded signal");
        {
            for (int type = 0; type < drive::numTypes; ++type)
            {
                for (const auto drivePct : { 0.0f, 50.0f, 100.0f })
                {
                    const auto out = runVoicing (type, 440.0, 0.9f, drivePct);

                    for (int i = 0; i < out.getNumSamples(); ++i)
                    {
                        const auto s = out.getSample (0, i);

                        expect (std::isfinite (s),
                                "type " + juce::String (type) + " went non-finite");
                        expect (std::abs (s) < 8.0f,
                                "type " + juce::String (type) + " ran away to "
                                    + juce::String (s));
                    }
                }
            }
        }

        beginTest ("The asymmetric curve itself is asymmetric");
        {
            // Isolated from the voicing chain, so a failure here points at the
            // curve rather than at a filter downstream of it.
            using milodikfx::dsp::clipDiodeAsymmetric;
            using milodikfx::dsp::clipCubic;

            expect (std::abs (clipDiodeAsymmetric (-0.5f)) > std::abs (clipDiodeAsymmetric (0.5f)),
                    "the negative half should turn over sooner");
            expectWithinAbsoluteError (std::abs (clipCubic (-0.5f)), std::abs (clipCubic (0.5f)), 1.0e-6f);

            juce::AudioBuffer<float> asym (1, 8192);
            juce::AudioBuffer<float> sym (1, 8192);
            fillSine (asym, bin (85), 0.6f);
            fillSine (sym, bin (85), 0.6f);

            for (int i = 0; i < asym.getNumSamples(); ++i)
            {
                asym.setSample (0, i, clipDiodeAsymmetric (asym.getSample (0, i) * 4.0f));
                sym.setSample (0, i, clipCubic (sym.getSample (0, i) * 4.0f));
            }

            const auto asymSecond = magnitudeAt (asym, bin (170), kWindow, kWindow);
            const auto symSecond = magnitudeAt (sym, bin (170), kWindow, kWindow);

            logMessage ("raw curve 2nd harmonic: asym " + juce::String (asymSecond, 6)
                        + " sym " + juce::String (symSecond, 6));

            expect (asymSecond > symSecond * 10.0,
                    "the curve alone produced no even harmonics");
        }

        beginTest ("A Tube Screamer keeps the bass out of the clipper");
        {
            // The circuit's whole character, measured directly rather than
            // through harmonics.
            //
            // A clipper compresses: play twice as loud into one and you do not
            // get twice as much out. Bass routed *around* the clipper stays
            // linear. So drive the same low note at two amplitudes and see
            // whether the fundamental tracks. A Tube Screamer splits at 720 Hz
            // and should track almost perfectly; the transparent voicing splits
            // at 40 Hz, so the same note goes through the clipper and squashes.
            //
            // Comparing harmonics instead would really be comparing the two
            // voicings' drive ranges and output trims, which is not the claim.
            const auto tsLinearity = lowNoteLinearity (drive::tubeScreamer);
            const auto klonLinearity = lowNoteLinearity (drive::transparent);

            logMessage ("low-note linearity: TS " + juce::String (tsLinearity, 3)
                        + " transparent " + juce::String (klonLinearity, 3)
                        + " (3.0 = perfectly clean)");

            expect (tsLinearity > klonLinearity * 1.3,
                    "the Tube Screamer compressed the bass as much as a full-range drive: "
                        + juce::String (tsLinearity, 3) + " vs " + juce::String (klonLinearity, 3));

            expect (tsLinearity > 2.4,
                    "the bass path should be close to linear, got "
                        + juce::String (tsLinearity, 3));
        }

        beginTest ("...but distorts the mids as hard as anything else");
        {
            // Same pedal, a note above the split. If it were simply a quiet
            // voicing rather than a split one, this would fail too.
            const auto ts = runVoicing (drive::tubeScreamer, bin (85), 0.7f, 90.0f);

            const auto fundamental = magnitudeAt (ts, bin (85), kWindow, kWindow);
            const auto third = magnitudeAt (ts, bin (255), kWindow, kWindow);

            expect (third > fundamental * 0.02,
                    "no harmonics at all above the split: " + juce::String (third, 5));
        }

        beginTest ("An OCD drives more low end than a Tube Screamer");
        {
            const auto ts = runVoicing (drive::tubeScreamer, bin (19), 0.7f, 90.0f);
            const auto ocd = runVoicing (drive::ocd, bin (19), 0.7f, 90.0f);

            // Same linearity measurement: the OCD's 200 Hz split sends far more
            // of a low note into the clipper, so it compresses where the Tube
            // Screamer stays clean.
            const auto tsLinearity = lowNoteLinearity (drive::tubeScreamer);
            const auto ocdLinearity = lowNoteLinearity (drive::ocd);

            expect (tsLinearity > ocdLinearity,
                    "OCD " + juce::String (ocdLinearity, 3)
                        + " vs TS " + juce::String (tsLinearity, 3));
        }

        beginTest ("Asymmetric voicings grow even harmonics, symmetric ones do not");
        {
            // A symmetric curve can only produce odd harmonics. The even ones
            // are the audible signature of an asymmetric clipper, and they are
            // what a Blues Driver or a Dumble-style drive is reached for.
            //
            // Measured at moderate drive on purpose. Pushed to full saturation
            // both halves flat-top into a square wave, the DC blocker centres
            // it, and the asymmetry stops producing even harmonics at all --
            // which is true of the real circuits too. The difference lives in
            // the range a player actually works in.
            const auto ts = runVoicing (drive::tubeScreamer, bin (85), 0.6f, 35.0f);
            const auto bd = runVoicing (drive::bluesDriver, bin (85), 0.6f, 35.0f);

            const auto tsSecond = magnitudeAt (ts, bin (170), kWindow, kWindow);
            const auto bdSecond = magnitudeAt (bd, bin (170), kWindow, kWindow);

            logMessage ("2nd harmonic: TS " + juce::String (tsSecond, 6)
                        + " (fundamental " + juce::String (magnitudeAt (ts, bin (85), kWindow, kWindow), 6)
                        + "), BD " + juce::String (bdSecond, 6)
                        + " (fundamental " + juce::String (magnitudeAt (bd, bin (85), kWindow, kWindow), 6) + ")");

            expect (bdSecond > tsSecond * 2.0,
                    "the asymmetric voicing produced no extra even harmonics: "
                        + juce::String (bdSecond, 5) + " vs " + juce::String (tsSecond, 5));
        }

        beginTest ("No voicing leaves DC behind");
        {
            // An asymmetric curve offsets the waveform, and letting that reach
            // the cabinet would shift its whole operating point.
            for (int type = 1; type < drive::numTypes; ++type)
            {
                const auto out = runVoicing (type, 500.0, 0.8f, 100.0f);

                double mean = 0.0;

                for (int i = 4096; i < out.getNumSamples(); ++i)
                    mean += out.getSample (0, i);

                mean /= 4096.0;

                expect (std::abs (mean) < 0.02,
                        "type " + juce::String (type) + " left DC at " + juce::String (mean, 5));
            }
        }

        beginTest ("A transparent drive at low gain gives back what it was given");
        {
            // The reason to own one. Clean blend plus a low-threshold curve
            // means the guitar still sounds like the guitar.
            const auto out = runVoicing (drive::transparent, bin (68), 0.2f, 15.0f);

            const auto fundamental = magnitudeAt (out, bin (68), kWindow, kWindow);
            const auto third = magnitudeAt (out, bin (204), kWindow, kWindow);

            expect (third < fundamental * 0.06,
                    "too dirty for a transparent voicing: "
                        + juce::String (third / fundamental, 4));
        }

        beginTest ("A clean boost barely saturates at all");
        {
            const auto out = runVoicing (drive::cleanBoost, bin (68), 0.25f, 60.0f);

            const auto fundamental = magnitudeAt (out, bin (68), kWindow, kWindow);
            const auto third = magnitudeAt (out, bin (204), kWindow, kWindow);

            expect (third < fundamental * 0.05,
                    "a clean boost should not be dirty: "
                        + juce::String (third / fundamental, 4));
        }

        beginTest ("Switching voicing does not jump in level");
        {
            // Otherwise auditioning types mid-song would be a volume ride.
            std::vector<float> levels;

            for (int type = 1; type < drive::numTypes; ++type)
            {
                const auto out = runVoicing (type, 440.0, 0.5f, 60.0f);
                const auto level = rmsOf (out, kWindow, kWindow);

                logMessage ("  type " + juce::String (type) + ": "
                            + juce::String (juce::Decibels::gainToDecibels (level), 2) + " dBFS");

                levels.push_back (level);
            }

            auto quietest = levels[0];
            auto loudest = levels[0];

            for (const auto level : levels)
            {
                quietest = juce::jmin (quietest, level);
                loudest = juce::jmax (loudest, level);
            }

            const auto spreadDb = juce::Decibels::gainToDecibels (loudest / juce::jmax (1.0e-6f, quietest));

            logMessage ("voicing level spread: " + juce::String (spreadDb, 2) + " dB");

            expect (spreadDb < 3.0,
                    "voicings are " + juce::String (spreadDb, 2) + " dB apart");
        }

        beginTest ("The Drive knob still crossfades to clean at the bottom");
        {
            for (int type = 1; type < drive::numTypes; ++type)
            {
                juce::AudioBuffer<float> input (1, 4096);
                fillSine (input, 440.0, 0.4f);

                const auto out = runVoicing (type, 440.0, 0.4f, 0.0f);

                for (int i = 2048; i < out.getNumSamples(); ++i)
                    expectWithinAbsoluteError (out.getSample (0, i),
                                               (float) (0.4 * std::sin (
                                                   juce::MathConstants<double>::twoPi * 440.0
                                                   * (double) i / kRate)),
                                               0.01f,
                                               "type " + juce::String (type)
                                                   + " coloured the signal at zero drive");
            }
        }

        beginTest ("Tone sweeps the top end, on the voicings that have one");
        {
            const auto dark = runWithTone (drive::tubeScreamer, 0.0f);
            const auto bright = runWithTone (drive::tubeScreamer, 100.0f);

            expect (bright > dark * 1.5,
                    "Tone did nothing: dark " + juce::String (dark, 5)
                        + " bright " + juce::String (bright, 5));
        }

        beginTest ("OCD's HP mode lets more low end through than LP");
        {
            const auto lp = runOcdMode (false);
            const auto hp = runOcdMode (true);

            expect (hp > lp,
                    "HP mode was not more aggressive: LP " + juce::String (lp, 5)
                        + " HP " + juce::String (hp, 5));
        }

        beginTest ("Every voicing survives every oversampling factor");
        {
            for (int type = 0; type < drive::numTypes; ++type)
            {
                for (int os = 0; os <= 3; ++os)
                {
                    const auto out = runVoicing (type, 440.0, 0.8f, 90.0f, os);

                    for (int i = 0; i < out.getNumSamples(); ++i)
                        expect (std::isfinite (out.getSample (0, i)),
                                "type " + juce::String (type) + " at oversampling "
                                    + juce::String (os) + " went non-finite");
                }
            }
        }

        beginTest ("A type index that does not exist is clamped, not indexed");
        {
            OverdriveProcessor od;

            od.setType (-5);
            expectEquals (od.getType(), 0);

            od.setType (9999);
            expectEquals (od.getType(), drive::numTypes - 1);
        }
    }

private:
    /**
     * How much the fundamental of a low note grows when the input is tripled.
     *
     * 3.0 means the low path is perfectly linear; a clipper compresses and
     * gives less. This is the split doing its job, measured without reference
     * to any voicing's drive range or output trim.
     */
    static double lowNoteLinearity (int type)
    {
        const auto quiet = runVoicing (type, bin (9), 0.2f, 70.0f);
        const auto loud = runVoicing (type, bin (9), 0.6f, 70.0f);

        const auto quietLevel = magnitudeAt (quiet, bin (9), kWindow, kWindow);
        const auto loudLevel = magnitudeAt (loud, bin (9), kWindow, kWindow);

        return quietLevel > 1.0e-9 ? loudLevel / quietLevel : 0.0;
    }

    /** Third-harmonic energy for a Tube Screamer at a given Tone setting. */
    static double runWithTone (int type, float tonePct)
    {
        milodikfx::dsp::OverdriveProcessor od;
        od.setEnabled (true);
        od.setType (type);
        od.setDrivePercent (90.0f);
        od.setLevelPercent (100.0f);
        od.setTonePercent (tonePct);
        od.setOversamplingIndex (1);
        od.prepareToPlay (kRate, 8192, 1);

        juce::AudioBuffer<float> buffer (1, 8192);

        for (int pass = 0; pass < 2; ++pass)
        {
            fillSine (buffer, bin (85), 0.7f);
            od.processBlock (buffer);
        }

        return magnitudeAt (buffer, bin (255), kWindow, kWindow);
    }

    /** Third-harmonic energy on a low note, LP versus HP mode. */
    static double runOcdMode (bool hp)
    {
        milodikfx::dsp::OverdriveProcessor od;
        od.setEnabled (true);
        od.setType (milodikfx::dsp::drive::ocd);
        od.setDrivePercent (90.0f);
        od.setLevelPercent (100.0f);
        od.setHighPeakMode (hp);
        od.setOversamplingIndex (1);
        od.prepareToPlay (kRate, 8192, 1);

        juce::AudioBuffer<float> buffer (1, 8192);

        for (int pass = 0; pass < 2; ++pass)
        {
            fillSine (buffer, bin (10), 0.7f);
            od.processBlock (buffer);
        }

        return magnitudeAt (buffer, bin (30), kWindow, kWindow);
    }
};

static DriveVoicingTests driveVoicingTests;
