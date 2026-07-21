#include "api/TunerHandler.h"

#include "api/ApiJson.h"

using namespace milodikfx::api;

namespace
{
juce::var readingToVar (const milodikfx::dsp::TunerAnalyzer& analyzer)
{
    const auto reading = analyzer.getReading();

    auto* object = new juce::DynamicObject();

    object->setProperty ("enabled", analyzer.isEnabled());
    object->setProperty ("note", milodikfx::dsp::TunerAnalyzer::describeNote (reading.midiNote));
    object->setProperty ("midiNote", reading.midiNote);
    object->setProperty ("frequency", reading.frequencyHz);
    object->setProperty ("cents", reading.cents);
    object->setProperty ("confidence", reading.confidence);

    // Whether the reading is worth showing at all. The UI would otherwise have
    // to duplicate the "is this a real note" rule and could disagree with the
    // engine about it.
    object->setProperty ("detected", reading.midiNote >= 0 && reading.confidence > 0.0f);

    return juce::var (object);
}
} // namespace

HttpHandler::Response TunerHandler::handleGet (const std::string&, const std::string&) const
{
    return jsonOk (readingToVar (analyzer));
}

HttpHandler::Response TunerHandler::handlePost (const std::string& path, const std::string& body)
{
    const auto segments = pathSegmentsAfter (path, "/api/tuner");

    if (segments.size() != 1 || segments[0] != "enable")
        return jsonError (404, "Unknown tuner endpoint");

    const auto parsed = parseBody (body);

    bool enabled = false;

    if (! readBool (parsed, "enabled", enabled))
        return jsonError (400, "Expected {\"enabled\": true|false}");

    analyzer.setEnabled (enabled);

    return jsonOk (readingToVar (analyzer));
}
