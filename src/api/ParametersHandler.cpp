#include "api/ParametersHandler.h"

#include "api/ApiJson.h"

using namespace milodikfx::api;

namespace
{
constexpr const char* kPrefix = "/api/parameters";

bool isMasterVolumePath (const std::string& path)
{
    return path == "/api/parameters/master-volume" || path == "/api/parameters/master-volume/";
}
} // namespace

HttpHandler::Response ParametersHandler::handleMasterVolumeGet() const
{
    const auto* parameter = registry_.findParameter (masterEffectId_, masterParameterId_);

    if (parameter == nullptr || ! parameter->get)
        return jsonError (503, "Master volume is not available");

    auto* object = new juce::DynamicObject();
    object->setProperty ("masterVolume", parameter->get());
    object->setProperty ("min", parameter->minValue);
    object->setProperty ("max", parameter->maxValue);

    return jsonOk (juce::var (object));
}

HttpHandler::Response ParametersHandler::handleMasterVolumePut (const juce::var& body) const
{
    double value = 0.0;

    if (! readNumber (body, "value", value))
        return jsonError (400, "Body must contain a numeric 'value'");

    float applied = 0.0f;

    if (! registry_.setParameter (masterEffectId_, masterParameterId_, (float) value, applied))
        return jsonError (503, "Master volume is not available");

    auto* object = new juce::DynamicObject();
    object->setProperty ("masterVolume", applied);

    return jsonOk (juce::var (object));
}

HttpHandler::Response ParametersHandler::handleGet (const std::string& path, const std::string&) const
{
    try
    {
        if (isMasterVolumePath (path))
            return handleMasterVolumeGet();

        if (path == kPrefix || path == "/api/parameters/")
            return jsonOk (registry_.toVar());

        const auto segments = pathSegmentsAfter (path, kPrefix);

        if (segments.size() != 2)
            return jsonError (404, "Expected /api/parameters/{effect}/{parameter}");

        const auto effectId = toLowerAscii (segments[0]);
        const auto* parameter = registry_.findParameter (effectId, segments[1]);

        if (parameter == nullptr || ! parameter->get)
            return jsonError (404, "Unknown parameter");

        auto* object = new juce::DynamicObject();
        object->setProperty ("effect", juce::String (effectId));
        object->setProperty ("parameter", juce::String (parameter->id));
        object->setProperty ("value", parameter->get());
        object->setProperty ("min", parameter->minValue);
        object->setProperty ("max", parameter->maxValue);
        object->setProperty ("unit", juce::String (parameter->unit));

        return jsonOk (juce::var (object));
    }
    catch (const std::exception& e)
    {
        return jsonError (500, juce::String ("Exception: ") + e.what());
    }
}

HttpHandler::Response ParametersHandler::handlePut (const std::string& path, const std::string& body)
{
    try
    {
        const auto parsed = parseBody (body);

        if (! parsed.isObject())
            return jsonError (400, "Body must be a JSON object");

        if (isMasterVolumePath (path))
            return handleMasterVolumePut (parsed);

        const auto segments = pathSegmentsAfter (path, kPrefix);

        if (segments.size() != 2)
            return jsonError (404, "Expected /api/parameters/{effect}/{parameter}");

        const auto effectId = toLowerAscii (segments[0]);

        double value = 0.0;

        if (! readNumber (parsed, "value", value))
            return jsonError (400, "Body must contain a numeric 'value'");

        float applied = 0.0f;

        if (! registry_.setParameter (effectId, segments[1], (float) value, applied))
            return jsonError (404, "Unknown parameter");

        auto* object = new juce::DynamicObject();
        object->setProperty ("effect", juce::String (effectId));
        object->setProperty ("parameter", juce::String (segments[1]));
        object->setProperty ("value", applied);

        return jsonOk (juce::var (object));
    }
    catch (const std::exception& e)
    {
        return jsonError (500, juce::String ("Exception: ") + e.what());
    }
}
