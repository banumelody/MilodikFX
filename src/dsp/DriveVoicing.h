#pragma once

#include <JuceHeader.h>

#include <cmath>

namespace milodikfx::dsp
{
/**
 * The eight drive voicings, as data rather than a branch per pedal.
 *
 * Every classic overdrive is the same three moves in a different order: decide
 * how much of the low end reaches the clipper, pick a clipping curve, then
 * shape what comes out. Modelling that as a table means adding a voicing is a
 * row, not a code path, and it keeps processBlock a single loop.
 *
 * The one that matters most is `splitHz`. A Tube Screamer's famous mid-hump is
 * not an EQ boost at all -- it is the bass being routed *around* the clipper and
 * added back clean, so only the mids and highs distort. Drop that corner and the
 * same circuit turns into an OCD; drop it to nothing and it turns transparent.
 */
enum class ClipCurve
{
    /** Cubic soft clip. Symmetric diode pair in the feedback loop. */
    cubic = 0,

    /** Gentler, higher headroom, very touch-sensitive. */
    tanhSoft,

    /** Linear well past unity then a fast corner: MOSFET to ground. */
    hardKnee,

    /** Positive half softer than the negative: asymmetric diode pair. */
    diodeAsymmetric
};

struct DriveVoicing
{
    /**
     * Corner below which the signal bypasses the clipper and is added back
     * clean. 0 means everything is clipped.
     */
    float splitHz = 0.0f;

    ClipCurve curve = ClipCurve::cubic;

    /** Built-in bias into the curve, on top of any user asymmetry. */
    float bias = 0.0f;

    /** Pre-gain the Drive knob commands at full. */
    float driveMax = 20.0f;

    /** Post-clip low-pass sweep. Both zero means the voicing has no Tone knob. */
    float toneMinHz = 0.0f;
    float toneMaxHz = 0.0f;

    /** Fixed presence lift after the clipper. */
    float presenceHz = 0.0f;
    float presenceDb = 0.0f;

    /** How much untouched signal is mixed back in. The Klon trick. */
    float cleanBlend = 0.0f;

    /** Level compensation so switching voicing does not jump in loudness. */
    float outputDb = 0.0f;

    /** Cascaded gain stages, 1 or 2. Two is what makes an amp-in-a-box. */
    int stages = 1;
};

namespace drive
{
/** Order is the enum the UI and presets store; append only, never reorder. */
enum Type
{
    custom = 0,
    tubeScreamer,
    bluesbreaker,
    bluesDriver,
    transparent,
    ocd,
    dumble,
    marshallInABox,
    cleanBoost,

    numTypes
};

/**
 * Values read off the circuits these are named for.
 *
 * The Tube Screamer's 720 Hz split is the well-documented one. The rest follow
 * the same reasoning: a Bluesbreaker passes more low end and clips far more
 * gently, an OCD passes almost all of it and clips harder, a Klon-style
 * transparent drive barely splits at all and leans on the clean blend instead.
 */
inline const DriveVoicing& voicingFor (int type) noexcept
{
    static const DriveVoicing table[numTypes] = {
        // custom -- never used; the untouched original path handles it.
        DriveVoicing {},

        // Tube Screamer: bass routed around the clipper, hence the mid-hump.
        // The high split costs real level, so it needs the most making up --
        // these output figures were measured, not guessed, so that auditioning
        // voicings is not a volume ride.
        DriveVoicing { 720.0f, ClipCurve::cubic, 0.0f, 40.0f,
                       1000.0f, 5000.0f, 0.0f, 0.0f, 0.0f, 1.2f, 1 },

        // Bluesbreaker: high headroom, barely any bias, opens up as you dig in.
        DriveVoicing { 300.0f, ClipCurve::tanhSoft, 0.05f, 12.0f,
                       1500.0f, 8000.0f, 3000.0f, 1.5f, 0.0f, -1.0f, 1 },

        // Blues Driver: discrete two-stage, asymmetric, bright and glassy.
        DriveVoicing { 150.0f, ClipCurve::diodeAsymmetric, 0.20f, 25.0f,
                       1200.0f, 7000.0f, 3500.0f, 2.5f, 0.0f, -2.0f, 2 },

        // Transparent: almost no split, hard low-threshold clip, clean blend
        // rising with gain. Bass and Treble replace the Tone knob.
        DriveVoicing { 40.0f, ClipCurve::hardKnee, 0.0f, 15.0f,
                       0.0f, 0.0f, 0.0f, 0.0f, 0.5f, -1.0f, 1 },

        // OCD: far more low end into a harder curve, and a lot more of it.
        DriveVoicing { 200.0f, ClipCurve::hardKnee, 0.12f, 60.0f,
                       900.0f, 6000.0f, 3000.0f, 1.0f, 0.0f, -3.0f, 1 },

        // Dumble-style: mid-focused split, asymmetric, dark post filter. Vocal.
        DriveVoicing { 500.0f, ClipCurve::diodeAsymmetric, 0.25f, 30.0f,
                       800.0f, 3500.0f, 0.0f, -1.0f, 0.0f, 1.0f, 2 },

        // Marshall-in-a-box: cascaded preamp gain into a passive-style stack.
        DriveVoicing { 120.0f, ClipCurve::cubic, 0.08f, 50.0f,
                       0.0f, 0.0f, 4000.0f, 3.0f, 0.0f, -4.0f, 2 },

        // Clean boost: barely any saturation at all, just the EP's colour.
        DriveVoicing { 0.0f, ClipCurve::tanhSoft, 0.0f, 3.0f,
                       0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1 },
    };

    return table[(size_t) juce::jlimit (0, numTypes - 1, type)];
}

inline bool isVoiced (int type) noexcept { return type > custom && type < numTypes; }
} // namespace drive

/** Cubic soft clip, normalised so saturation lands at +/-1. */
inline float clipCubic (float x) noexcept
{
    if (std::abs (x) <= 1.0f)
        return 1.5f * (x - (x * x * x) / 3.0f);

    return x > 0.0f ? 1.0f : -1.0f;
}

inline float clipTanhSoft (float x) noexcept
{
    return std::tanh (x);
}

/** Linear well past unity, then a fast corner. MOSFET clipping to ground. */
inline float clipHardKnee (float x) noexcept
{
    constexpr float knee = 0.7f;

    const auto a = std::abs (x);

    if (a <= knee)
        return x;

    const auto over = a - knee;
    const auto y = juce::jmin (1.0f, knee + over / (1.0f + over * 3.0f));

    return x > 0.0f ? y : -y;
}

/** Asymmetric diode pair: the negative half turns over sooner and harder. */
inline float clipDiodeAsymmetric (float x) noexcept
{
    return x >= 0.0f ? std::tanh (x) : std::tanh (x * 1.6f) * 0.78f;
}

inline float applyCurve (ClipCurve curve, float x) noexcept
{
    switch (curve)
    {
        case ClipCurve::tanhSoft:        return clipTanhSoft (x);
        case ClipCurve::hardKnee:        return clipHardKnee (x);
        case ClipCurve::diodeAsymmetric: return clipDiodeAsymmetric (x);
        case ClipCurve::cubic:
        default:                         return clipCubic (x);
    }
}

/**
 * One-pole DC blocker.
 *
 * An asymmetric curve produces a DC offset on a symmetric input, and letting
 * that reach the cabinet would shift its whole operating point. Real pedals put
 * a coupling capacitor here for the same reason. One pole rather than a biquad
 * because the corner is 15 Hz against a rate that can be 384 kHz when 8x
 * oversampling is on, where a biquad's coefficients get uncomfortably close to
 * the limits of float.
 */
struct DcBlocker
{
    float x1 = 0.0f;
    float y1 = 0.0f;
    float r = 0.999f;

    void prepare (double sampleRate) noexcept
    {
        constexpr double cornerHz = 15.0;
        r = (float) juce::jlimit (0.9, 0.99999,
                                  1.0 - (juce::MathConstants<double>::twoPi * cornerHz
                                         / juce::jmax (1.0, sampleRate)));
    }

    void reset() noexcept { x1 = y1 = 0.0f; }

    inline float process (float x) noexcept
    {
        const auto y = x - x1 + r * y1;
        x1 = x;
        y1 = std::isfinite (y) ? y : 0.0f;
        return y1;
    }
};
} // namespace milodikfx::dsp
