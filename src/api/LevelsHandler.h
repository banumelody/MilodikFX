#pragma once

#include <JuceHeader.h>

#include <atomic>

#include "api/HttpHandler.h"

/**
 * GET /api/levels - realtime metering.
 *
 * Written from the audio thread through plain relaxed atomic stores, read from
 * connection threads. Nothing here blocks or allocates on the audio side.
 */
class LevelsHandler final : public HttpHandler
{
public:
    LevelsHandler() = default;

    Response handleGet (const std::string& path, const std::string& query) const override;

    /**
     * @param inputDb       what the interface delivered, before the trim
     * @param chainInputDb  what the chain actually receives, after the trim
     *
     * Both, because they answer different questions. The trimmed figure is what
     * the Input knob is dialled against -- without it the knob would have no
     * feedback at all, since the meter is measured before the chain runs. The
     * untrimmed one still shows a signal arriving too hot from the interface,
     * which no amount of digital trim can fix.
     */
    void updateLevels (float inputDb, float chainInputDb, float outputDb) noexcept
    {
        inputLevel.store (inputDb, std::memory_order_relaxed);
        chainInputLevel.store (chainInputDb, std::memory_order_relaxed);
        outputLevel.store (outputDb, std::memory_order_relaxed);
    }

    void updateGainReduction (float gateGainValue, float compressorDb, float limiterDb) noexcept
    {
        gateGain.store (gateGainValue, std::memory_order_relaxed);
        compressorReductionDb.store (compressorDb, std::memory_order_relaxed);
        limiterReductionDb.store (limiterDb, std::memory_order_relaxed);
    }

    void updateLoad (float cpuPercent, double sampleRate, int bufferSize) noexcept
    {
        cpuLoadPercent.store (cpuPercent, std::memory_order_relaxed);
        currentSampleRate.store (sampleRate, std::memory_order_relaxed);
        currentBufferSize.store (bufferSize, std::memory_order_relaxed);
    }

    void setAudioRunning (bool running) noexcept
    {
        audioRunning.store (running, std::memory_order_relaxed);
    }

    /**
     * Bumped whenever the chain changes in a way the UI did not initiate --
     * a footswitch recalling a scene, a MIDI CC moving a parameter, a program
     * change loading a preset. It rides along in the meter payload (already
     * streaming ~22 Hz), so the UI can notice and refetch without a new
     * connection or a new poll. Deliberately NOT bumped on ordinary parameter
     * writes: a UI knob drag would otherwise tell every client to refetch and
     * fight the drag it just made.
     */
    void bumpChainVersion() noexcept
    {
        chainVersion.fetch_add (1, std::memory_order_relaxed);
    }

    juce::uint32 getChainVersion() const noexcept
    {
        return chainVersion.load (std::memory_order_relaxed);
    }

private:
    // Silence floor, reported until the audio callback delivers the first block.
    // Keep in sync with MainComponent::kMeterFloorDb.
    static constexpr float kFloorDb = -100.0f;

    std::atomic<float> inputLevel { kFloorDb };
    std::atomic<float> chainInputLevel { kFloorDb };
    std::atomic<float> outputLevel { kFloorDb };
    std::atomic<float> gateGain { 1.0f };
    std::atomic<float> compressorReductionDb { 0.0f };
    std::atomic<float> limiterReductionDb { 0.0f };
    std::atomic<float> cpuLoadPercent { 0.0f };
    std::atomic<double> currentSampleRate { 0.0 };
    std::atomic<int> currentBufferSize { 0 };
    std::atomic<bool> audioRunning { false };
    std::atomic<juce::uint32> chainVersion { 0 };
};
