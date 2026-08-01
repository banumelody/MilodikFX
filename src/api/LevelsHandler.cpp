#include "api/LevelsHandler.h"

#include "api/ApiJson.h"
#include "dsp/LooperProcessor.h"

using namespace milodikfx::api;

HttpHandler::Response LevelsHandler::handleGet (const std::string&, const std::string&) const
{
    auto* object = new juce::DynamicObject();

    object->setProperty ("inputLevel", inputLevel.load (std::memory_order_relaxed));
    object->setProperty ("chainInputLevel", chainInputLevel.load (std::memory_order_relaxed));
    object->setProperty ("outputLevel", outputLevel.load (std::memory_order_relaxed));

    // Per channel alongside the combined figures, never instead of them.
    object->setProperty ("inputLevelL", inputLevelL.load (std::memory_order_relaxed));
    object->setProperty ("inputLevelR", inputLevelR.load (std::memory_order_relaxed));
    object->setProperty ("chainInputLevelL", chainInputLevelL.load (std::memory_order_relaxed));
    object->setProperty ("chainInputLevelR", chainInputLevelR.load (std::memory_order_relaxed));
    object->setProperty ("outputLevelL", outputLevelL.load (std::memory_order_relaxed));
    object->setProperty ("outputLevelR", outputLevelR.load (std::memory_order_relaxed));
    object->setProperty ("gateGain", gateGain.load (std::memory_order_relaxed));
    object->setProperty ("compressorReductionDb", compressorReductionDb.load (std::memory_order_relaxed));
    object->setProperty ("limiterReductionDb", limiterReductionDb.load (std::memory_order_relaxed));
    object->setProperty ("cpuPercent", cpuLoadPercent.load (std::memory_order_relaxed));
    object->setProperty ("sampleRate", currentSampleRate.load (std::memory_order_relaxed));
    object->setProperty ("bufferSize", currentBufferSize.load (std::memory_order_relaxed));
    object->setProperty ("audioRunning", audioRunning.load (std::memory_order_relaxed));
    object->setProperty ("floorDb", kFloorDb);
    object->setProperty ("chainVersion", (int) chainVersion.load (std::memory_order_relaxed));

    // Only when there is a looper to report. The field is absent otherwise, so a
    // client can tell "no looper in this build" from "a looper sitting empty".
    if (looperPresent.load (std::memory_order_relaxed))
    {
        using Looper = milodikfx::dsp::LooperProcessor;

        auto* looper = new juce::DynamicObject();
        looper->setProperty ("state", Looper::toString ((Looper::State) looperState.load (std::memory_order_relaxed)));
        looper->setProperty ("hasLoop", looperHasLoop.load (std::memory_order_relaxed));
        looper->setProperty ("loopSeconds", looperSeconds.load (std::memory_order_relaxed));
        looper->setProperty ("position", looperPosition.load (std::memory_order_relaxed));
        looper->setProperty ("level", looperLevel.load (std::memory_order_relaxed));
        looper->setProperty ("maxSeconds", (double) Looper::kMaxSeconds);

        object->setProperty ("looper", juce::var (looper));
    }

    // Compact: this is the meter payload, delivered ~22 times a second down the
    // SSE stream. One line is fewer bytes and skips the line-splitting the
    // stream writer would otherwise do on a pretty-printed body.
    return jsonOkCompact (juce::var (object));
}
