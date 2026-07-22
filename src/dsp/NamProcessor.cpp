#include "dsp/NamProcessor.h"

#include <cmath>

#if MILODIKFX_ENABLE_NAM
 // NAM headers pull in Eigen, so they live only in this translation unit.
 #include "NAM/dsp.h"
 #include "NAM/get_dsp.h"
#endif

namespace milodikfx::dsp
{
/**
 * A loaded model together with everything needed to run it.
 *
 * Built entirely on a worker thread and published as one atomic pointer, so the
 * audio thread never sees a half-configured model or an unprepared resampler.
 */
struct NamProcessor::Slot
{
   #if MILODIKFX_ENABLE_NAM
    std::unique_ptr<nam::DSP> model;
   #endif
    NamResampler resampler;
    double modelRate = 48000.0;
    juce::String name;
};

namespace
{
/** The convention: a model that declares no rate is assumed to be 48 kHz. */
constexpr double kAssumedModelRate = 48000.0;
} // namespace

NamProcessor::NamProcessor() = default;

NamProcessor::~NamProcessor()
{
    // Runs on the message thread after the audio callback has stopped, so
    // deleting these here is not an audio-thread free.
    delete active;
    delete staged.exchange (nullptr);
    delete retired.exchange (nullptr);
}

bool NamProcessor::isAvailable() noexcept
{
   #if MILODIKFX_ENABLE_NAM
    return juce::SystemStats::hasAVX2();
   #else
    return false;
   #endif
}

juce::String NamProcessor::unavailableReason()
{
   #if MILODIKFX_ENABLE_NAM
    return juce::SystemStats::hasAVX2()
               ? juce::String()
               : juce::String ("CPU ini tidak mendukung AVX2 - model NAM tidak bisa dijalankan");
   #else
    return "Dukungan NAM tidak dikompilasi dalam build ini";
   #endif
}

void NamProcessor::prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels)
{
    juce::ignoreUnused (numChannels);

    hostRate.store (sampleRate > 0.0 ? sampleRate : 48000.0, std::memory_order_relaxed);
    hostMaxBlock.store (juce::jmax (1, samplesPerBlock), std::memory_order_relaxed);

    smoothedInput.reset (sampleRate > 0.0 ? sampleRate : 48000.0, kSmoothingSeconds,
                         juce::Decibels::decibelsToGain (inputDb.load (std::memory_order_relaxed)));
    smoothedOutput.reset (sampleRate > 0.0 ? sampleRate : 48000.0, kSmoothingSeconds,
                          juce::Decibels::decibelsToGain (outputDb.load (std::memory_order_relaxed)));

    // Audio is not running here (the device is (re)starting on the message
    // thread), so it is safe to touch the audio-thread-owned pointers. Fold any
    // staged model into active, reap the retired slot, then re-prepare active
    // for the new rate/block.
    collectGarbage();

    if (auto* incoming = staged.exchange (nullptr))
    {
        delete active;
        active = incoming;
    }

    if (active != nullptr)
    {
        reconfigureSlot (*active);

        loadedRate.store (active->resampler.isPassthrough() ? hostRate.load (std::memory_order_relaxed)
                                                            : active->modelRate,
                          std::memory_order_relaxed);
        loadedLatency.store (active->resampler.getLatencySamples(), std::memory_order_relaxed);
    }

    prepared = true;
}

void NamProcessor::reconfigureSlot (Slot& slot)
{
    const auto rate = hostRate.load (std::memory_order_relaxed);
    const auto maxBlock = hostMaxBlock.load (std::memory_order_relaxed);

    slot.resampler.prepare (rate, slot.modelRate, maxBlock);

   #if MILODIKFX_ENABLE_NAM
    if (slot.model != nullptr)
    {
        // Reset with a max block size at least as large as the resampler will
        // ever hand it, so process() never grows its internal buffers.
        const auto maxModel = slot.resampler.isPassthrough()
                                  ? maxBlock
                                  : slot.resampler.getMaxModelFrames();

        // prewarm belongs off the audio thread; Reset does it by default and we
        // are on the message thread here.
        slot.model->Reset (slot.modelRate, juce::jmax (1, maxModel));
    }
   #endif
}

void NamProcessor::adoptStagedIfReady() noexcept
{
    // Only take a new model when the retired slot is empty, so at most one slot
    // is ever waiting to be reaped -- no queue, no audio-thread free.
    if (retired.load (std::memory_order_acquire) != nullptr)
        return;

    auto* incoming = staged.exchange (nullptr, std::memory_order_acquire);

    if (incoming == nullptr)
        return;

    auto* old = active;
    active = incoming;

    // The reaper (collectGarbage) will delete this off the audio thread.
    retired.store (old, std::memory_order_release);
}

void NamProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (! prepared)
        return;

    adoptStagedIfReady();

    if (! enabled.load (std::memory_order_relaxed) || active == nullptr)
    {
        // Keep the smoothers tracking so re-enabling does not jump.
        smoothedInput.snapTo (juce::Decibels::decibelsToGain (inputDb.load (std::memory_order_relaxed)));
        smoothedOutput.snapTo (juce::Decibels::decibelsToGain (outputDb.load (std::memory_order_relaxed)));
        return;
    }

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    const auto inTarget = juce::Decibels::decibelsToGain (inputDb.load (std::memory_order_relaxed));
    const auto outTarget = juce::Decibels::decibelsToGain (outputDb.load (std::memory_order_relaxed));

    auto* mono = buffer.getWritePointer (0);

    // Drive into the model, at host rate, smoothed so a knob turn does not step.
    for (int i = 0; i < numSamples; ++i)
        mono[i] *= smoothedInput.next (inTarget);

    // Resample down, run the model, resample back up -- all in place on channel 0.
    active->resampler.process (mono, numSamples,
                               [this] (const float* in, float* out, int frames)
                               {
                                  #if MILODIKFX_ENABLE_NAM
                                   if (active->model != nullptr)
                                   {
                                       auto* inPtr = const_cast<float*> (in);
                                       float* inPtrs[1] = { inPtr };
                                       float* outPtrs[1] = { out };
                                       active->model->process (inPtrs, outPtrs, frames);
                                   }
                                   else
                                  #endif
                                   {
                                       // No model (stub build): straight copy so
                                       // the resampler still round-trips cleanly.
                                       std::copy (in, in + frames, out);
                                   }
                               });

    // Makeup, then guard: an unfamiliar model on a hot input could produce a
    // sample past full scale, and this sits before the master limiter.
    for (int i = 0; i < numSamples; ++i)
    {
        auto s = mono[i] * smoothedOutput.next (outTarget);

        if (! std::isfinite (s))
            s = 0.0f;

        mono[i] = s;
    }

    // The amp is mono; the rest of the pre-cabinet chain fed both channels the
    // same signal, so copy channel 0 across rather than modelling twice.
    for (int ch = 1; ch < juce::jmin (numChannels, kMaxChannels); ++ch)
        std::copy (mono, mono + numSamples, buffer.getWritePointer (ch));

    // A model with more than two channels would leave the rest untouched; the
    // engine only ever runs stereo, so there is nothing beyond channel 1.
}

void NamProcessor::reset()
{
    smoothedInput.snapTo (juce::Decibels::decibelsToGain (inputDb.load (std::memory_order_relaxed)));
    smoothedOutput.snapTo (juce::Decibels::decibelsToGain (outputDb.load (std::memory_order_relaxed)));

    if (active != nullptr)
        active->resampler.reset();
}

void NamProcessor::collectGarbage()
{
    // Whichever thread wins the exchange owns the pointer and deletes it; the
    // other gets null. Safe to call from the timer and from a loader thread.
    delete retired.exchange (nullptr, std::memory_order_acquire);
}

std::unique_ptr<NamProcessor::Slot> NamProcessor::buildSlot (const juce::File& file, juce::String& outError)
{
   #if MILODIKFX_ENABLE_NAM
    if (! juce::SystemStats::hasAVX2())
    {
        outError = unavailableReason();
        return nullptr;
    }

    if (! file.existsAsFile())
    {
        outError = "Berkas model tidak ditemukan";
        return nullptr;
    }

    auto slot = std::make_unique<Slot>();

    try
    {
        slot->model = nam::get_dsp (std::filesystem::path (file.getFullPathName().toStdString()));
    }
    catch (const std::exception& e)
    {
        outError = juce::String ("Gagal memuat model: ") + e.what();
        return nullptr;
    }
    catch (...)
    {
        outError = "Gagal memuat model NAM";
        return nullptr;
    }

    if (slot->model == nullptr)
    {
        outError = "Berkas ini bukan model NAM yang valid";
        return nullptr;
    }

    const auto declared = slot->model->GetExpectedSampleRate();
    slot->modelRate = declared > 0.0 ? declared : kAssumedModelRate;
    slot->name = file.getFileNameWithoutExtension();

    // Prepare the resampler and Reset+prewarm the model here, off the audio
    // thread, so the published slot is ready to run as-is.
    reconfigureSlot (*slot);

    return slot;
   #else
    juce::ignoreUnused (file);
    outError = unavailableReason();
    return nullptr;
   #endif
}

juce::String NamProcessor::loadModel (const juce::File& file)
{
    // Reap any earlier retired slot first, so back-to-back loads collect
    // themselves without waiting for the timer.
    collectGarbage();

    juce::String error;
    auto slot = buildSlot (file, error);

    if (slot == nullptr)
        return error.isNotEmpty() ? error : juce::String ("Gagal memuat model NAM");

    const auto rate = slot->resampler.isPassthrough() ? hostRate.load (std::memory_order_relaxed)
                                                      : slot->modelRate;
    const auto latency = slot->resampler.getLatencySamples();
    const auto name = slot->name;

    // Publish. The exchange hands us any previously-staged-but-unconsumed slot,
    // which we then own and can delete off this (non-audio) thread.
    auto* previous = staged.exchange (slot.release(), std::memory_order_release);
    delete previous;

    loadedRate.store (rate, std::memory_order_relaxed);
    loadedLatency.store (latency, std::memory_order_relaxed);
    modelLoaded.store (true, std::memory_order_relaxed);

    {
        const juce::ScopedLock lock (nameLock);
        loadedName = name;
    }

    return {};
}

void NamProcessor::clearModel()
{
    collectGarbage();

    // Stage a model-less slot. When the audio thread adopts it, the real model
    // becomes the retired slot and the reaper frees it -- so clearing a model is
    // the same safe pointer-swap path as loading one, never an audio-thread free.
    // A slot with no model makes the resampler's callback a straight copy, i.e.
    // passthrough.
    const auto rate = hostRate.load (std::memory_order_relaxed);
    const auto maxBlock = hostMaxBlock.load (std::memory_order_relaxed);

    auto empty = std::make_unique<Slot>();
    empty->modelRate = rate;
    empty->resampler.prepare (rate, rate, maxBlock);

    auto* previous = staged.exchange (empty.release(), std::memory_order_release);
    delete previous;

    modelLoaded.store (false, std::memory_order_relaxed);

    {
        const juce::ScopedLock lock (nameLock);
        loadedName.clear();
    }
}

juce::String NamProcessor::getLoadedName() const
{
    const juce::ScopedLock lock (nameLock);
    return loadedName;
}

void NamProcessor::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable, std::memory_order_relaxed);
}

bool NamProcessor::isEnabled() const noexcept
{
    return enabled.load (std::memory_order_relaxed);
}

void NamProcessor::setInputDb (float db) noexcept
{
    inputDb.store (juce::jlimit (-24.0f, 24.0f, db), std::memory_order_relaxed);
}

float NamProcessor::getInputDb() const noexcept
{
    return inputDb.load (std::memory_order_relaxed);
}

void NamProcessor::setOutputDb (float db) noexcept
{
    outputDb.store (juce::jlimit (-24.0f, 24.0f, db), std::memory_order_relaxed);
}

float NamProcessor::getOutputDb() const noexcept
{
    return outputDb.load (std::memory_order_relaxed);
}
} // namespace milodikfx::dsp
