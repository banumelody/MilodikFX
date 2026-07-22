#pragma once

#include <JuceHeader.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace milodikfx::dsp
{
/**
 * Runs a mono processor at a different sample rate than the host.
 *
 * NAM models are trained at a fixed rate — 48 kHz for almost all of them —
 * while this engine runs at 96 kHz. Feeding a 48 kHz model at 96 kHz does not
 * pitch-shift the audio, but it halves every internal time constant, so the
 * amp's whole voicing shifts brighter and thinner. It is wrong, not merely
 * approximate. So the host block is resampled down to the model's rate, the
 * model runs, and the result is resampled back up.
 *
 * When the host rate already equals the model rate this is a straight
 * passthrough — no interpolation, no latency.
 *
 * Realtime contract: prepare() allocates everything (two windowed-sinc
 * interpolators, three fixed scratch/FIFO buffers). process() allocates
 * nothing. The FIFOs are primed with silence at prepare time so that from the
 * very first block the resamplers always have enough history — no zeros are
 * ever injected mid-stream, which would click.
 *
 * A JUCE interpolator rather than the reference plugin's AudioDSPTools
 * ResamplingContainer: that pulls in WDL and, worse, has std::cerr and throw
 * reachable from its per-block path, which this codebase forbids anywhere the
 * audio thread can reach.
 */
class NamResampler
{
public:
    NamResampler() = default;

    /**
     * @param hostSampleRate   the rate the engine runs at
     * @param modelSampleRate  the rate the model expects
     * @param maxHostBlock      the largest block process() will ever be handed
     */
    void prepare (double hostSampleRate, double modelSampleRate, int maxHostBlock);

    void reset();

    /**
     * Resamples `numHostFrames` down, calls `model`, resamples back up in place.
     *
     * `model` is invoked once per block as model(const float* in, float* out,
     * int frames), with a frame count of at most getMaxModelFrames(). Templated
     * rather than taking a std::function so a capturing callback can never
     * allocate on the audio thread.
     */
    template <typename ModelCallback>
    void process (float* hostBuffer, int numHostFrames, ModelCallback&& model)
    {
        if (hostBuffer == nullptr || numHostFrames <= 0)
            return;

        numHostFrames = juce::jmin (numHostFrames, maxHostFrames);

        if (passthrough)
        {
            model (hostBuffer, hostBuffer, numHostFrames);
            return;
        }

        const auto modelFrames = downsample (hostBuffer, numHostFrames);

        if (modelFrames > 0)
        {
            model (modelIn.data(), modelOut.data(), modelFrames);

            for (int i = 0; i < modelFrames; ++i)
                if (! std::isfinite (modelOut[(size_t) i]))
                    modelOut[(size_t) i] = 0.0f;
        }

        upsample (hostBuffer, numHostFrames, modelFrames);
    }

    /** True when host and model rates match and process() is a straight passthrough. */
    bool isPassthrough() const noexcept { return passthrough; }

    /** The largest model-rate block the model will be asked for. Size Reset() to this. */
    int getMaxModelFrames() const noexcept { return maxModelFrames; }

    /** Round-trip latency the resampling adds, in host-rate samples (0 when passthrough). */
    int getLatencySamples() const noexcept { return latencySamples; }

private:
    /** Host block -> model frames, in place into modelIn. Returns model frame count. */
    int downsample (const float* hostBuffer, int numHostFrames);

    /** Model frames (from modelOut) -> exactly numHostFrames back into hostBuffer. */
    void upsample (float* hostBuffer, int numHostFrames, int modelFrames);

    /** Runs an impulse through the round trip to find the real group delay. */
    int measureLatency();

    // Reserved so a fractional ratio that briefly needs a few extra input
    // samples can never run the FIFO dry after priming.
    static constexpr int kFifoMargin = 64;

    double hostRate = 48000.0;
    double modelRate = 48000.0;
    double downRatio = 1.0; // host / model, >1 when downsampling
    double upRatio = 1.0;   // model / host
    bool passthrough = true;

    int maxHostFrames = 0;
    int maxModelFrames = 0;
    int latencySamples = 0;

    // CatmullRom (4-point cubic) rather than WindowedSinc: JUCE's windowed sinc
    // evaluates 201 taps per output sample, and run twice per block it cost more
    // than the model itself -- the whole chain plus a Standard model measured
    // 46 % of budget, most of it here. CatmullRom is 4 taps, transparent enough
    // for a guitar signal (the reference plugin uses a 25-tap Lanczos), and the
    // round-trip correlation test guards the quality. This dropped the total to
    // comfortably within budget; see docs/nam-plan.md.
    juce::CatmullRomInterpolator downsampler;
    juce::CatmullRomInterpolator upsampler;

    // Host samples waiting to be turned into model samples; model samples
    // waiting to be turned back into host samples. Both fixed-capacity.
    std::vector<float> downFifo;
    std::vector<float> upFifo;
    int downFill = 0;
    int upFill = 0;

    std::vector<float> modelIn;
    std::vector<float> modelOut;
};
} // namespace milodikfx::dsp
