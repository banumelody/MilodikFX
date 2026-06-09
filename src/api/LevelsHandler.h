#pragma once

#include "HttpHandler.h"
#include <JuceHeader.h>
#include <atomic>

/**
 * Handles /api/levels endpoint for real-time level metering.
 * GET /api/levels - Returns current input/output levels in dB
 */
class LevelsHandler final : public HttpHandler
{
public:
    explicit LevelsHandler() = default;

    Response handleGet(const std::string& path, const std::string& query) const override;
    
    // Called from audio thread to update levels
    void updateLevels(float inputDb, float outputDb)
    {
        inputLevel_.store(inputDb, std::memory_order_relaxed);
        outputLevel_.store(outputDb, std::memory_order_relaxed);
    }

private:
    mutable std::atomic<float> inputLevel_{ -60.0f };
    mutable std::atomic<float> outputLevel_{ -60.0f };
};
