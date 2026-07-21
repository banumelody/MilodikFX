#include "api/DevicesHandler.h"

#include "api/ApiJson.h"

using namespace milodikfx::api;

namespace
{
juce::var snapshotToVar (const milodikfx::audio::AudioDeviceSnapshot& snapshot)
{
    auto* object = new juce::DynamicObject();

    object->setProperty ("open", snapshot.isOpen);
    object->setProperty ("type", snapshot.typeName);
    object->setProperty ("inputDevice", snapshot.inputDeviceName);
    object->setProperty ("outputDevice", snapshot.outputDeviceName);
    object->setProperty ("sampleRate", snapshot.sampleRate);
    object->setProperty ("bufferSize", snapshot.bufferSize);
    object->setProperty ("inputChannels", snapshot.inputChannels);
    object->setProperty ("outputChannels", snapshot.outputChannels);
    object->setProperty ("inputLatencyMs", snapshot.inputLatencyMs);
    object->setProperty ("outputLatencyMs", snapshot.outputLatencyMs);
    object->setProperty ("roundTripLatencyMs", snapshot.roundTripLatencyMs);
    object->setProperty ("lowLatency", snapshot.isLowLatencyType);

    if (snapshot.lastError.isNotEmpty())
        object->setProperty ("error", snapshot.lastError);

    return juce::var (object);
}
} // namespace

HttpHandler::Response DevicesHandler::handleGet (const std::string&, const std::string&) const
{
    try
    {
        auto* root = new juce::DynamicObject();
        root->setProperty ("current", snapshotToVar (controller.getSnapshot()));
        root->setProperty ("available", controller.describeAvailable());

        return jsonOk (juce::var (root));
    }
    catch (const std::exception& e)
    {
        return jsonError (500, juce::String ("Exception: ") + e.what());
    }
}

HttpHandler::Response DevicesHandler::handlePost (const std::string& path, const std::string& body)
{
    try
    {
        const auto segments = pathSegmentsAfter (path, "/api/devices");

        if (! segments.empty() && toLowerAscii (segments[0]) == "optimise")
        {
            const auto error = controller.optimiseForLowLatency();

            if (error.isNotEmpty())
                return jsonError (400, error);

            auto* root = new juce::DynamicObject();
            root->setProperty ("current", snapshotToVar (controller.getSnapshot()));

            return jsonOk (juce::var (root));
        }

        const auto parsed = parseBody (body);

        if (! parsed.isObject())
            return jsonError (400, "Body must be a JSON object");

        milodikfx::audio::AudioDeviceRequest request;

        request.typeName = parsed["type"].toString().trim();
        request.inputDeviceName = parsed["inputDevice"].toString().trim();
        request.outputDeviceName = parsed["outputDevice"].toString().trim();

        double sampleRate = 0.0;
        if (readNumber (parsed, "sampleRate", sampleRate))
            request.sampleRate = sampleRate;

        double bufferSize = 0.0;
        if (readNumber (parsed, "bufferSize", bufferSize))
            request.bufferSize = (int) bufferSize;

        const auto error = controller.applyRequest (request);

        if (error.isNotEmpty())
            return jsonError (400, error);

        auto* root = new juce::DynamicObject();
        root->setProperty ("current", snapshotToVar (controller.getSnapshot()));

        return jsonOk (juce::var (root));
    }
    catch (const std::exception& e)
    {
        return jsonError (500, juce::String ("Exception: ") + e.what());
    }
}
