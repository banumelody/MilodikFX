#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <memory>

#include "dsp/AudioProcessorBase.h"
#include "dsp/Biquad.h"
#include "dsp/NamResampler.h"

// Forward-declare the NAM DSP type so this header does not drag Eigen into every
// translation unit that includes it. Only NamProcessor.cpp touches nam::DSP.
namespace nam { class DSP; }

namespace milodikfx::dsp
{
/**
 * The amp head: a Neural Amp Modeler (.nam) captured amp, run in the chain.
 *
 * Sits after the tone shaping and before the cabinet, exactly where a real head
 * sits between the pedals and the speaker. The cabinet IR loader already models
 * the speaker; this models everything the IR fundamentally cannot — the way an
 * amp's distortion changes with how hard you play.
 *
 * Realtime discipline:
 *
 *  - A model is loaded (file read, JSON parse, weight allocation, prewarm) on a
 *    worker/message thread, never the audio thread — the same rule as the IR
 *    loader. The loader builds a complete Slot (the model plus a resampler
 *    already prepared for that model's rate) and publishes it atomically.
 *
 *  - The audio thread only ever does a pointer swap: it adopts a staged slot and
 *    hands the old one to a reaper. Nothing is allocated or freed on the audio
 *    thread. The reference NAM plugin frees the retired model inside its audio
 *    callback; this deliberately does not.
 *
 *  - process() is allocation-free once a model is loaded (verified upstream with
 *    an allocation-tracking test). The model is Reset() with a max block size
 *    large enough that it never grows its internal buffers.
 *
 * Without a model loaded, or when NAM was not compiled in, it is a pure
 * passthrough.
 */
class NamProcessor final : public AudioProcessorBase
{
public:
    NamProcessor();
    ~NamProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels) override;
    void processBlock (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setEnabled (bool shouldEnable) noexcept;
    bool isEnabled() const noexcept;

    /** Drive into the model, in dB. Most models expect roughly unity. */
    void setInputDb (float db) noexcept;
    float getInputDb() const noexcept;

    /** Makeup after the model, in dB. */
    void setOutputDb (float db) noexcept;
    float getOutputDb() const noexcept;

    /**
     * Loads a .nam model. Call from a worker/message thread; never the audio
     * thread. Returns an empty string on success, otherwise a reason.
     */
    juce::String loadModel (const juce::File& file);

    /** Drops the current model; process() then passes through. */
    void clearModel();

    bool isModelLoaded() const noexcept { return modelLoaded.load (std::memory_order_relaxed); }
    juce::String getLoadedName() const;

    /** The model's declared sample rate, or 48000 when it did not declare one. */
    double getModelSampleRate() const noexcept { return loadedRate.load (std::memory_order_relaxed); }

    /** Round-trip latency the resampling adds, in host samples. */
    int getLatencySamples() const noexcept { return loadedLatency.load (std::memory_order_relaxed); }

    /** Whether this build can run models at all (compiled in + CPU has AVX2). */
    static bool isAvailable() noexcept;
    static juce::String unavailableReason();

    /**
     * Deletes any retired model. Called off the audio thread — from
     * MainComponent's timer and at the start of each load — so a swapped-out
     * model is freed by a reaper rather than in the callback.
     */
    void collectGarbage();

private:
    struct Slot;

    /** Builds a fully-prepared slot for `file` at the current host settings. */
    std::unique_ptr<Slot> buildSlot (const juce::File& file, juce::String& outError);

    /** Re-prepares a slot's resampler and model for the current host settings. */
    void reconfigureSlot (Slot& slot);

    /** Audio-thread: adopt a staged slot if the retired slot is free. */
    void adoptStagedIfReady() noexcept;

    static constexpr float kSmoothingSeconds = 0.02f;
    static constexpr int kMaxChannels = 2;

    std::atomic<bool> enabled { false };
    std::atomic<float> inputDb { 0.0f };
    std::atomic<float> outputDb { 0.0f };

    // Host settings, set in prepareToPlay, read by the loader.
    std::atomic<double> hostRate { 48000.0 };
    std::atomic<int> hostMaxBlock { 512 };
    bool prepared = false;

    // The three-slot handoff. `active` is audio-thread-owned. `staged` and
    // `retired` are the only cross-thread pointers, moved by atomic exchange.
    Slot* active = nullptr;
    std::atomic<Slot*> staged { nullptr };
    std::atomic<Slot*> retired { nullptr };

    // API-visible state, published by the loader so REST reads never touch the
    // audio-thread-owned `active` pointer.
    std::atomic<bool> modelLoaded { false };
    std::atomic<double> loadedRate { 48000.0 };
    std::atomic<int> loadedLatency { 0 };
    juce::CriticalSection nameLock;
    juce::String loadedName;

    // Audio-thread-owned smoothing and mono scratch.
    SmoothedParam smoothedInput;
    SmoothedParam smoothedOutput;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NamProcessor)
};
} // namespace milodikfx::dsp
