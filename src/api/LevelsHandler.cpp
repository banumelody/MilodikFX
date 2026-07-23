#include "api/LevelsHandler.h"

#include "api/ApiJson.h"

using namespace milodikfx::api;

HttpHandler::Response LevelsHandler::handleGet (const std::string&, const std::string&) const
{
    auto* object = new juce::DynamicObject();

    object->setProperty ("inputLevel", inputLevel.load (std::memory_order_relaxed));
    object->setProperty ("chainInputLevel", chainInputLevel.load (std::memory_order_relaxed));
    object->setProperty ("outputLevel", outputLevel.load (std::memory_order_relaxed));
    object->setProperty ("gateGain", gateGain.load (std::memory_order_relaxed));
    object->setProperty ("compressorReductionDb", compressorReductionDb.load (std::memory_order_relaxed));
    object->setProperty ("limiterReductionDb", limiterReductionDb.load (std::memory_order_relaxed));
    object->setProperty ("cpuPercent", cpuLoadPercent.load (std::memory_order_relaxed));
    object->setProperty ("sampleRate", currentSampleRate.load (std::memory_order_relaxed));
    object->setProperty ("bufferSize", currentBufferSize.load (std::memory_order_relaxed));
    object->setProperty ("audioRunning", audioRunning.load (std::memory_order_relaxed));
    object->setProperty ("floorDb", kFloorDb);
    object->setProperty ("chainVersion", (int) chainVersion.load (std::memory_order_relaxed));

    // Compact: this is the meter payload, delivered ~22 times a second down the
    // SSE stream. One line is fewer bytes and skips the line-splitting the
    // stream writer would otherwise do on a pretty-printed body.
    return jsonOkCompact (juce::var (object));
}
