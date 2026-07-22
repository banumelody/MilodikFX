#include "dsp/NamResampler.h"

#include <algorithm>
#include <cmath>

namespace milodikfx::dsp
{
namespace
{
/** Rates within this of each other count as equal: no resampling. */
constexpr double kRateEpsilon = 0.5;

/**
 * Model-rate samples of silence pre-loaded into the up FIFO.
 *
 * This is what lets the upsampler produce a full host block from the very first
 * process() call without ever running dry — running dry would inject zeros
 * mid-stream, which clicks. It is also the bulk of the reported latency.
 */
constexpr int kPrimeModelFrames = 32;
} // namespace

void NamResampler::prepare (double hostSampleRate, double modelSampleRate, int maxHostBlock)
{
    hostRate = hostSampleRate > 0.0 ? hostSampleRate : 48000.0;
    modelRate = modelSampleRate > 0.0 ? modelSampleRate : 48000.0;
    maxHostFrames = juce::jmax (1, maxHostBlock);

    passthrough = std::abs (hostRate - modelRate) < kRateEpsilon;

    downRatio = hostRate / modelRate;
    upRatio = modelRate / hostRate;

    if (passthrough)
    {
        // Nothing to resample. modelIn/out still sized so process() can hand the
        // host buffer straight to the model without touching the FIFOs.
        maxModelFrames = maxHostFrames;
        latencySamples = 0;

        modelIn.assign ((size_t) maxModelFrames, 0.0f);
        modelOut.assign ((size_t) maxModelFrames, 0.0f);
        downFifo.clear();
        upFifo.clear();
        return;
    }

    // Most model frames a single host block can turn into, with headroom.
    maxModelFrames = (int) std::ceil ((double) maxHostFrames / downRatio) + 2;

    modelIn.assign ((size_t) maxModelFrames, 0.0f);
    modelOut.assign ((size_t) maxModelFrames, 0.0f);

    // Capacity: a block's worth plus whatever the fractional ratio leaves behind.
    downFifo.assign ((size_t) (maxHostFrames * 2 + kFifoMargin), 0.0f);
    upFifo.assign ((size_t) (maxModelFrames + kPrimeModelFrames + kFifoMargin), 0.0f);

    reset();

    // Measure the real round-trip group delay rather than estimating it: the
    // windowed-sinc kernels add far more than the priming alone, and a reported
    // figure that disagreed with what actually comes out would be a fiction.
    latencySamples = measureLatency();

    reset();
}

int NamResampler::measureLatency()
{
    // Push an impulse through an identity model and find where its peak emerges.
    const auto probe = juce::jmin (maxHostFrames, 64);
    const auto totalBlocks = 512;

    std::vector<float> block ((size_t) probe, 0.0f);

    auto outputIndex = 0;
    auto peakIndex = 0;
    auto peak = 0.0f;
    auto impulseSent = false;

    for (int b = 0; b < totalBlocks; ++b)
    {
        std::fill (block.begin(), block.end(), 0.0f);

        if (! impulseSent)
        {
            block[0] = 1.0f;
            impulseSent = true;
        }

        const auto modelFrames = downsample (block.data(), probe);

        // Identity model: model output mirrors model input.
        std::copy (modelIn.begin(), modelIn.begin() + modelFrames, modelOut.begin());

        upsample (block.data(), probe, modelFrames);

        for (int i = 0; i < probe; ++i)
        {
            if (std::abs (block[(size_t) i]) > peak)
            {
                peak = std::abs (block[(size_t) i]);
                peakIndex = outputIndex;
            }

            ++outputIndex;
        }
    }

    return peakIndex;
}

void NamResampler::reset()
{
    downsampler.reset();
    upsampler.reset();

    downFill = 0;

    if (passthrough)
    {
        upFill = 0;
        return;
    }

    std::fill (downFifo.begin(), downFifo.end(), 0.0f);
    std::fill (upFifo.begin(), upFifo.end(), 0.0f);

    // Prime the up FIFO with silence so the first block already has enough
    // model-rate history to produce a full host block.
    upFill = juce::jmin (kPrimeModelFrames, (int) upFifo.size());
}

int NamResampler::downsample (const float* hostBuffer, int numHostFrames)
{
    // Append the host block, then produce as many model frames as the FIFO can
    // safely supply without the interpolator running past its input.
    const auto downCapacity = (int) downFifo.size();
    const auto toAppend = juce::jmin (numHostFrames, downCapacity - downFill);

    std::copy (hostBuffer, hostBuffer + toAppend, downFifo.begin() + downFill);
    downFill += toAppend;

    auto wantModel = (int) std::llround ((double) numHostFrames / downRatio);
    wantModel = juce::jlimit (0, maxModelFrames, wantModel);

    // Cap to what the FIFO holds so process() never has to inject zeros.
    const auto safeModel = (int) std::floor ((double) downFill / downRatio);
    wantModel = juce::jmin (wantModel, safeModel);

    if (wantModel <= 0)
        return 0;

    const auto used = downsampler.process (downRatio,
                                           downFifo.data(),
                                           modelIn.data(),
                                           wantModel,
                                           downFill,
                                           0);

    const auto consumed = juce::jlimit (0, downFill, used);

    if (consumed > 0 && consumed < downFill)
        std::copy (downFifo.begin() + consumed, downFifo.begin() + downFill, downFifo.begin());

    downFill -= consumed;

    return wantModel;
}

void NamResampler::upsample (float* hostBuffer, int numHostFrames, int modelFrames)
{
    // Append the model output, then produce exactly the host block back.
    const auto upCapacity = (int) upFifo.size();
    const auto upAppend = juce::jmin (modelFrames, upCapacity - upFill);

    std::copy (modelOut.begin(), modelOut.begin() + upAppend, upFifo.begin() + upFill);
    upFill += upAppend;

    const auto used = upsampler.process (upRatio,
                                         upFifo.data(),
                                         hostBuffer,
                                         numHostFrames,
                                         upFill,
                                         0);

    const auto consumed = juce::jlimit (0, upFill, used);

    if (consumed > 0 && consumed < upFill)
        std::copy (upFifo.begin() + consumed, upFifo.begin() + upFill, upFifo.begin());

    upFill -= consumed;
}
} // namespace milodikfx::dsp
