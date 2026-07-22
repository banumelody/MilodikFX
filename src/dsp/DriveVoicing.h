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
    diodeAsymmetric,

    /** Low threshold, rounded knee: a germanium diode. The Centaur's edge. */
    germanium,

    /** Op-amp into silicon diodes to ground: harder and gnarlier than hardKnee.
        The Pro Co RAT's grind. */
    hardClip
};

/** How a voicing's post-clip tone control behaves. */
enum class ToneMode
{
    /** A single low-pass sweep. What most drive and distortion pedals have. */
    lpSweep = 0,

    /** A tilt between a low-pass and a high-pass with a scooped middle -- the
        Big Muff tone stack, deepest notch at noon. */
    midScoopTilt
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

    /**
     * A treble/mid lift applied *before* the clipper.
     *
     * A RAT is not just a harder OCD: its LM308 gain network emphasises mids
     * and treble going in, so those frequencies clip while the lows stay
     * cleaner. Zero dB means no pre-emphasis, which is every other voicing.
     */
    float preEmphasisHz = 0.0f;
    float preEmphasisDb = 0.0f;

    /** Post-clip low-pass sweep. Both zero means the voicing has no Tone knob. */
    float toneMinHz = 0.0f;
    float toneMaxHz = 0.0f;

    /** Shape of the Tone control; midScoopTilt is the Big Muff stack. */
    ToneMode toneMode = ToneMode::lpSweep;

    /** Reverses the Tone knob, so clockwise is darker -- the RAT's Filter. */
    bool toneReversed = false;

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
    centaur,
    rat,
    bigMuff,

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
    // Designated initialisers rather than positional, so a new field in the
    // struct can never silently shift every voicing's values out of alignment.
    static const DriveVoicing table[numTypes] = {
        // custom -- never used; the untouched original path handles it.
        DriveVoicing {},

        // Tube Screamer: bass routed around the clipper, hence the mid-hump.
        // The high split costs real level, so it needs the most making up --
        // these output figures were measured, not guessed, so that auditioning
        // voicings is not a volume ride.
        DriveVoicing { .splitHz = 720.0f, .curve = ClipCurve::cubic, .driveMax = 40.0f,
                       .toneMinHz = 1000.0f, .toneMaxHz = 5000.0f, .outputDb = 1.2f },

        // Bluesbreaker: high headroom, barely any bias, opens up as you dig in.
        DriveVoicing { .splitHz = 300.0f, .curve = ClipCurve::tanhSoft, .bias = 0.05f, .driveMax = 12.0f,
                       .toneMinHz = 1500.0f, .toneMaxHz = 8000.0f,
                       .presenceHz = 3000.0f, .presenceDb = 1.5f, .outputDb = -1.0f },

        // Blues Driver: discrete two-stage, asymmetric, bright and glassy.
        DriveVoicing { .splitHz = 150.0f, .curve = ClipCurve::diodeAsymmetric, .bias = 0.20f, .driveMax = 25.0f,
                       .toneMinHz = 1200.0f, .toneMaxHz = 7000.0f,
                       .presenceHz = 3500.0f, .presenceDb = 2.5f, .outputDb = -2.0f, .stages = 2 },

        // Transparent (Timmy/Klon-style): almost no split, hard low-threshold
        // clip, clean blend rising with gain. Bass and Treble replace the Tone.
        DriveVoicing { .splitHz = 40.0f, .curve = ClipCurve::hardKnee, .driveMax = 15.0f,
                       .cleanBlend = 0.5f, .outputDb = -1.0f },

        // OCD: far more low end into a harder curve, and a lot more of it.
        DriveVoicing { .splitHz = 200.0f, .curve = ClipCurve::hardKnee, .bias = 0.12f, .driveMax = 60.0f,
                       .toneMinHz = 900.0f, .toneMaxHz = 6000.0f,
                       .presenceHz = 3000.0f, .presenceDb = 1.0f, .outputDb = -3.0f },

        // Dumble-style: mid-focused split, asymmetric, dark post filter. Vocal.
        DriveVoicing { .splitHz = 500.0f, .curve = ClipCurve::diodeAsymmetric, .bias = 0.25f, .driveMax = 30.0f,
                       .toneMinHz = 800.0f, .toneMaxHz = 3500.0f, .outputDb = 1.0f, .stages = 2 },

        // Marshall-in-a-box: cascaded preamp gain into a passive-style stack.
        DriveVoicing { .splitHz = 120.0f, .curve = ClipCurve::cubic, .bias = 0.08f, .driveMax = 50.0f,
                       .presenceHz = 4000.0f, .presenceDb = 3.0f, .outputDb = -4.0f, .stages = 2 },

        // Clean boost: barely any saturation at all, just the EP's colour.
        DriveVoicing { .curve = ClipCurve::tanhSoft, .driveMax = 3.0f },

        // Centaur (Klon-style): germanium soft clip, a low split so the lows stay
        // clean, and a big clean blend that rises with gain -- the "transparent"
        // trick. Sits beside Transparent (which leans Timmy) rather than
        // replacing it, since presets depend on that one's sound.
        DriveVoicing { .splitHz = 100.0f, .curve = ClipCurve::germanium, .driveMax = 30.0f,
                       .toneMinHz = 2000.0f, .toneMaxHz = 8000.0f,
                       .presenceHz = 2500.0f, .presenceDb = 1.5f, .cleanBlend = 0.6f, .outputDb = -1.0f },

        // RAT: an LM308 gain stage that emphasises mids and treble *before* a
        // hard silicon clip -- the pre-emphasis is what makes it a RAT and not
        // just a higher-gain OCD. The Filter knob runs backwards: clockwise is
        // darker. Highest gain of the lot.
        DriveVoicing { .splitHz = 100.0f, .curve = ClipCurve::hardClip, .bias = 0.08f, .driveMax = 120.0f,
                       .preEmphasisHz = 1200.0f, .preEmphasisDb = 6.0f,
                       .toneMinHz = 800.0f, .toneMaxHz = 5000.0f, .toneReversed = true, .outputDb = -6.0f },

        // Big Muff: two cascaded gain stages driven hard into a woolly fuzz --
        // no split, so the whole low end is clipped -- and the scooped-mid tone
        // stack that is its signature, deepest notch at noon.
        // The mid scoop and the two-stage compression both cost level, so the
        // Muff needs the most making up of any voicing -- measured, so switching
        // to it is not a drop in volume.
        DriveVoicing { .splitHz = 0.0f, .curve = ClipCurve::tanhSoft, .driveMax = 50.0f,
                       .toneMinHz = 900.0f, .toneMode = ToneMode::midScoopTilt,
                       .outputDb = 4.0f, .stages = 2 },
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

/**
 * Germanium diode: conducts earlier and approaches the rail gradually.
 *
 * An algebraic soft clip -- never quite flat -- so it saturates sooner and more
 * gently than silicon. The compressed, rounded feel a Klon Centaur is reached
 * for, though most of that pedal's "transparency" is really its clean blend.
 */
inline float clipGermanium (float x) noexcept
{
    const auto s = x * 1.5f;
    return s / (1.0f + std::abs (s));
}

/**
 * Silicon diodes to ground after an op-amp: a hard clip with a sharp corner,
 * harsher than the MOSFET knee. The Pro Co RAT's grind. The corner is only
 * lightly rounded, enough to take the very worst of the aliasing off an ideal
 * clip without softening the edge that is the whole point.
 */
inline float clipHardClip (float x) noexcept
{
    constexpr float knee = 0.9f;

    const auto a = std::abs (x);

    if (a <= knee)
        return x;

    const auto over = a - knee;
    const auto y = juce::jmin (1.0f, knee + over / (1.0f + over * 8.0f));

    return x > 0.0f ? y : -y;
}

inline float applyCurve (ClipCurve curve, float x) noexcept
{
    switch (curve)
    {
        case ClipCurve::tanhSoft:        return clipTanhSoft (x);
        case ClipCurve::hardKnee:        return clipHardKnee (x);
        case ClipCurve::diodeAsymmetric: return clipDiodeAsymmetric (x);
        case ClipCurve::germanium:       return clipGermanium (x);
        case ClipCurve::hardClip:        return clipHardClip (x);
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
