#include "api/MidiHandler.h"

#include "api/ApiJson.h"

using namespace milodikfx::api;
using milodikfx::midi::MappingMode;
using milodikfx::midi::MappingKind;
using milodikfx::midi::Mapping;

namespace
{
juce::String describeMode (MappingMode mode)
{
    return mode == MappingMode::toggle ? "toggle" : "continuous";
}

/** Anything that is not "toggle" is continuous; there are only the two. */
MappingMode parseMode (const juce::var& value)
{
    return value.toString().trim().equalsIgnoreCase ("toggle") ? MappingMode::toggle
                                                              : MappingMode::continuous;
}

juce::String kindString (MappingKind kind)
{
    switch (kind)
    {
        case MappingKind::scene:   return "scene";
        case MappingKind::channel: return "channel";
        default:                   return "parameter";
    }
}

/** Builds a mapping from a request body, defaulting to a parameter target. */
Mapping parseMapping (const juce::var& parsed)
{
    Mapping mapping;

    const auto kind = parsed["kind"].toString().trim().toLowerCase();
    const auto index = parsed["index"].isVoid() ? -1 : (int) parsed["index"];

    if (kind == "scene")
    {
        mapping.kind = MappingKind::scene;
        mapping.index = index;
    }
    else if (kind == "channel")
    {
        mapping.kind = MappingKind::channel;
        mapping.effectId = parsed["effect"].toString().trim();
        mapping.index = index;
    }
    else
    {
        mapping.kind = MappingKind::parameter;
        mapping.effectId = parsed["effect"].toString().trim();
        mapping.parameterId = parsed["parameter"].toString().trim();
        mapping.mode = parseMode (parsed["mode"]);
    }

    return mapping;
}

juce::var mappingToVar (int ccNumber, const Mapping& mapping)
{
    auto* object = new juce::DynamicObject();

    object->setProperty ("cc", ccNumber);
    object->setProperty ("kind", kindString (mapping.kind));
    object->setProperty ("effect", mapping.effectId);
    object->setProperty ("parameter", mapping.parameterId);
    object->setProperty ("index", mapping.index);
    object->setProperty ("mode", describeMode (mapping.mode));

    return juce::var (object);
}

/** Parses the CC number out of "/api/midi/mappings/17". -1 when it is not one. */
int parseControllerNumber (const std::string& path)
{
    const auto segments = pathSegmentsAfter (path, "/api/midi");

    if (segments.size() != 2 || segments[0] != "mappings")
        return -1;

    const juce::String text (segments[1]);

    if (text.isEmpty() || ! text.containsOnly ("0123456789"))
        return -1;

    const auto number = text.getIntValue();

    return number >= 0 && number < milodikfx::midi::MidiController::kNumControllers ? number : -1;
}
} // namespace

HttpHandler::Response MidiHandler::describeState() const
{
    auto* object = new juce::DynamicObject();

    juce::Array<juce::var> devices;

    for (const auto& name : milodikfx::midi::MidiController::listDevices())
        devices.add (name);

    object->setProperty ("devices", devices);
    object->setProperty ("current", controller.getOpenDeviceName());
    object->setProperty ("open", controller.isOpen());

    juce::Array<juce::var> mappings;

    for (const auto& [cc, mapping] : controller.getMappings())
        mappings.add (mappingToVar (cc, mapping));

    object->setProperty ("mappings", mappings);

    const auto learning = controller.getLearnTarget();

    object->setProperty ("learning", learning.isValid() ? mappingToVar (-1, learning) : juce::var());

    // Echoing the last controller seen is what makes an unresponsive setup
    // diagnosable: it separates "nothing is arriving" from "it arrives but is
    // not mapped".
    object->setProperty ("lastCc", controller.getLastControllerNumber());
    object->setProperty ("lastValue", controller.getLastControllerValue());

    return jsonOk (juce::var (object));
}

HttpHandler::Response MidiHandler::handleGet (const std::string& path, const std::string&) const
{
    const auto segments = pathSegmentsAfter (path, "/api/midi");

    if (! segments.empty())
        return jsonError (404, "Unknown MIDI endpoint");

    return describeState();
}

HttpHandler::Response MidiHandler::handlePost (const std::string& path, const std::string& body)
{
    const auto segments = pathSegmentsAfter (path, "/api/midi");

    if (segments.size() != 1)
        return jsonError (404, "Unknown MIDI endpoint");

    const auto parsed = parseBody (body);

    if (segments[0] == "device")
    {
        if (! parsed.isObject())
            return jsonError (400, "Expected {\"name\": \"...\"}");

        const auto name = parsed["name"].toString();
        const auto error = controller.openDevice (name);

        if (error.isNotEmpty())
            return jsonError (400, error);

        // Remember what the user picked so the app can keep it open across a
        // wireless drop. Empty name = "none", which also stops reconnecting.
        if (onDeviceChosen)
            onDeviceChosen (name);

        return describeState();
    }

    if (segments[0] == "learn")
    {
        Mapping target;

        if (parsed.isObject())
            target = parseMapping (parsed);

        // An empty body disarms rather than erroring: closing the panel while
        // learn is armed has to be able to cancel it.
        if (! target.isValid())
        {
            controller.stopLearning();
            return describeState();
        }

        controller.startLearning (target);

        return describeState();
    }

    return jsonError (404, "Unknown MIDI endpoint");
}

HttpHandler::Response MidiHandler::handlePut (const std::string& path, const std::string& body)
{
    const auto cc = parseControllerNumber (path);

    if (cc < 0)
        return jsonError (404, "Expected /api/midi/mappings/<0-127>");

    const auto parsed = parseBody (body);

    if (! parsed.isObject())
        return jsonError (400, "Expected a mapping object");

    const auto mapping = parseMapping (parsed);

    if (! mapping.isValid())
        return jsonError (400, "Mapping is incomplete for its kind");

    controller.setMapping (cc, mapping);

    return describeState();
}

HttpHandler::Response MidiHandler::handleDelete (const std::string& path)
{
    const auto cc = parseControllerNumber (path);

    if (cc < 0)
        return jsonError (404, "Expected /api/midi/mappings/<0-127>");

    controller.clearMapping (cc);

    return describeState();
}
