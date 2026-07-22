// Benchmark spike for P2-1: measures NAM model inference against MilodikFX's
// realtime budget.
//
// The host runs ASIO at 96 kHz with a 32-sample block: 333 us of wall time per
// block. NAM models run at their trained rate (almost always 48 kHz), so after
// the mandatory 96<->48 resampling each host block hands the model exactly
// 16 frames. The number that decides everything is therefore: how long does
// process() take for a 16-frame call, in a Release /MT /arch:AVX2 build, on
// this machine?
//
// Also measured: load and Reset/prewarm time (the model-switch pause a user
// would feel), and larger frame counts to show how per-block overhead scales.

#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "NAM/dsp.h"
#include "NAM/get_dsp.h"

namespace
{
using Clock = std::chrono::steady_clock;

double millisecondsSince (Clock::time_point start)
{
    return std::chrono::duration<double, std::milli> (Clock::now() - start).count();
}

/// One host block at 96 kHz / 32 samples, in microseconds.
constexpr double kHostBlockUs = 32.0 * 1'000'000.0 / 96'000.0; // 333.3

struct BlockResult
{
    int frames = 0;
    double meanUs = 0.0;
    double p99Us = 0.0;
    double maxUs = 0.0;
};

BlockResult measureBlockSize (nam::DSP& model, int frames, int iterations)
{
    std::vector<float> input ((size_t) frames, 0.0f);
    std::vector<float> output ((size_t) frames, 0.0f);

    float* inPtrs[1] = { input.data() };
    float* outPtrs[1] = { output.data() };

    // A guitar-ish signal rather than silence: a gate-free network should not
    // care, but measuring the denormal-free happy path is the honest baseline.
    auto fill = [&input, frames] (int block)
    {
        for (int i = 0; i < frames; ++i)
            input[(size_t) i] = 0.25f * std::sin (0.03f * (float) (block * frames + i));
    };

    // Warm-up: first-touch faults, branch predictors, whatever Eigen lazily
    // initialises. Not measured.
    for (int block = 0; block < 512; ++block)
    {
        fill (block);
        model.process (inPtrs, outPtrs, frames);
    }

    std::vector<double> samples ((size_t) iterations, 0.0);

    // The sink defeats dead-code elimination of the whole loop.
    volatile float sink = 0.0f;

    for (int block = 0; block < iterations; ++block)
    {
        fill (block);

        const auto start = Clock::now();
        model.process (inPtrs, outPtrs, frames);
        samples[(size_t) block] = std::chrono::duration<double, std::micro> (Clock::now() - start).count();

        sink = sink + output[0];
    }

    (void) sink;

    std::sort (samples.begin(), samples.end());

    BlockResult result;
    result.frames = frames;

    double sum = 0.0;
    for (const auto s : samples)
        sum += s;

    result.meanUs = sum / (double) iterations;
    result.p99Us = samples[(size_t) ((double) iterations * 0.99)];
    result.maxUs = samples.back();

    return result;
}

int runModel (const std::string& path)
{
    std::printf ("\n=== %s\n", path.c_str());

    // --- Load: file read + JSON parse + weight alloc. Never on the audio thread.
    const auto loadStart = Clock::now();

    std::unique_ptr<nam::DSP> model;

    try
    {
        model = nam::get_dsp (std::filesystem::path (path));
    }
    catch (const std::exception& e)
    {
        std::printf ("  LOAD FAILED: %s\n", e.what());
        return 1;
    }

    const auto loadMs = millisecondsSince (loadStart);

    const auto expectedRate = model->GetExpectedSampleRate();

    std::printf ("  load (get_dsp)      : %8.1f ms\n", loadMs);
    std::printf ("  expected sample rate: %8.0f Hz%s\n",
                 expectedRate, expectedRate < 0.0 ? " (unknown -> assume 48k)" : "");

    // --- Reset + prewarm: allocation and a long warm-up run. Also never on the
    // audio thread; this is the model-switch pause a user would feel.
    const auto modelRate = expectedRate > 0.0 ? expectedRate : 48000.0;

    const auto resetStart = Clock::now();
    model->Reset (modelRate, 128); // max frames the host could ever hand it
    const auto resetMs = millisecondsSince (resetStart);

    std::printf ("  Reset + prewarm     : %8.1f ms\n", resetMs);

    // --- The decisive measurement. 16 frames is what one 32-sample 96 kHz host
    // block becomes after resampling to the model's 48 kHz.
    std::printf ("  budget: one host block (32 smp @ 96 kHz) = %.0f us\n", kHostBlockUs);

    for (const auto frames : { 16, 32, 64, 128 })
    {
        const auto r = measureBlockSize (*model, frames, 20000);

        // Frames beyond 16 correspond to proportionally more host time.
        const auto budgetUs = kHostBlockUs * (double) frames / 16.0;

        std::printf ("  frames %3d: mean %7.1f us  p99 %7.1f us  max %7.1f us  -> %5.1f%% of budget (p99 %5.1f%%)\n",
                     r.frames, r.meanUs, r.p99Us, r.maxUs,
                     100.0 * r.meanUs / budgetUs,
                     100.0 * r.p99Us / budgetUs);
    }

    return 0;
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::printf ("usage: nambench <model.nam> [more.nam ...]\n");
        return 2;
    }

    std::printf ("NAM inference benchmark -- Release /MT /arch:AVX2, NAM_SAMPLE=float\n");
    std::printf ("host budget: 32 samples @ 96 kHz = %.0f us per block\n", kHostBlockUs);

    auto failures = 0;

    for (int i = 1; i < argc; ++i)
        failures += runModel (argv[i]);

    return failures == 0 ? 0 : 1;
}
