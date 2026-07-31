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

/** The physical jacks, plus which one currently feeds each engine channel. */
juce::var inputRoutingToVar (const milodikfx::audio::AudioDeviceController& controller)
{
    juce::Array<juce::var> ports;

    for (const auto& port : controller.listInputPorts())
    {
        auto* entry = new juce::DynamicObject();
        entry->setProperty ("name", port.name);

        // A port the device is not currently streaming cannot be chosen; saying
        // so here means the UI does not have to guess from the channel count.
        entry->setProperty ("available", port.callbackPosition >= 0);
        ports.add (juce::var (entry));
    }

    auto* routing = new juce::DynamicObject();
    routing->setProperty ("ports", ports);
    routing->setProperty ("left", controller.getInputPortName (false));
    routing->setProperty ("right", controller.getInputPortName (true));

    return juce::var (routing);
}
} // namespace

HttpHandler::Response DevicesHandler::handleGet (const std::string&, const std::string&) const
{
    try
    {
        auto* root = new juce::DynamicObject();
        root->setProperty ("current", snapshotToVar (controller.getSnapshot()));
        root->setProperty ("available", controller.describeAvailable());
        root->setProperty ("inputRouting", inputRoutingToVar (controller));

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

        // Port routing arrives on the same endpoint as the rest of the device
        // setup, and is applied first: it does not reopen anything, so a body
        // carrying both a new device and a new routing lands the routing on the
        // device that is about to be opened rather than the one on its way out.
        if (parsed.hasProperty ("inputPortLeft") || parsed.hasProperty ("inputPortRight"))
        {
            const auto left = parsed.hasProperty ("inputPortLeft")
                                ? parsed["inputPortLeft"].toString().trim()
                                : controller.getInputPortName (false);
            const auto right = parsed.hasProperty ("inputPortRight")
                                 ? parsed["inputPortRight"].toString().trim()
                                 : controller.getInputPortName (true);

            controller.setInputPortNames (left, right);
        }

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
        root->setProperty ("inputRouting", inputRoutingToVar (controller));

        return jsonOk (juce::var (root));
    }
    catch (const std::exception& e)
    {
        return jsonError (500, juce::String ("Exception: ") + e.what());
    }
}
