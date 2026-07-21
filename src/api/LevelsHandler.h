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

    void updateLevels (float inputDb, float outputDb) noexcept
    {
        inputLevel.store (inputDb, std::memory_order_relaxed);
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

private:
    // Silence floor, reported until the audio callback delivers the first block.
    // Keep in sync with MainComponent::kMeterFloorDb.
    static constexpr float kFloorDb = -100.0f;

    std::atomic<float> inputLevel { kFloorDb };
    std::atomic<float> outputLevel { kFloorDb };
    std::atomic<float> gateGain { 1.0f };
    std::atomic<float> compressorReductionDb { 0.0f };
    std::atomic<float> limiterReductionDb { 0.0f };
    std::atomic<float> cpuLoadPercent { 0.0f };
    std::atomic<double> currentSampleRate { 0.0 };
    std::atomic<int> currentBufferSize { 0 };
    std::atomic<bool> audioRunning { false };
};
