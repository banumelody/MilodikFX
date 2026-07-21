#include "api/EffectsHandler.h"

#include "api/ApiJson.h"

using namespace milodikfx::api;

namespace
{
constexpr const char* kPrefix = "/api/effects";
} // namespace

HttpHandler::Response EffectsHandler::handleGet (const std::string& path, const std::string&) const
{
    try
    {
        if (path == kPrefix || path == "/api/effects/")
            return jsonOk (registry_.toVar());

        const auto segments = pathSegmentsAfter (path, kPrefix);

        if (segments.empty())
            return jsonError (404, "Expected /api/effects/{effect}");

        const auto effectId = toLowerAscii (segments[0]);
        const auto* effect = registry_.findEffect (effectId);

        if (effect == nullptr)
            return jsonError (404, "Unknown effect");

        if (segments.size() == 1)
            return jsonOk (registry_.effectToVar (*effect));

        if (segments.size() == 2 && segments[1] == "enabled")
        {
            auto* object = new juce::DynamicObject();
            object->setProperty ("effect", juce::String (effectId));
            object->setProperty ("enabled", effect->isEnabled ? effect->isEnabled() : true);

            return jsonOk (juce::var (object));
        }

        if (segments.size() == 2)
        {
            const auto* parameter = registry_.findParameter (effectId, segments[1]);

            if (parameter == nullptr || ! parameter->get)
                return jsonError (404, "Unknown parameter");

            auto* object = new juce::DynamicObject();
            object->setProperty ("effect", juce::String (effectId));
            object->setProperty ("parameter", juce::String (parameter->id));
            object->setProperty ("value", parameter->get());

            return jsonOk (juce::var (object));
        }

        return jsonError (404, "Unknown effects path");
    }
    catch (const std::exception& e)
    {
        return jsonError (500, juce::String ("Exception: ") + e.what());
    }
}

HttpHandler::Response EffectsHandler::handlePost (const std::string& path, const std::string& body)
{
    try
    {
        const auto segments = pathSegmentsAfter (path, kPrefix);

        if (segments.empty())
            return jsonError (404, "Expected /api/effects/{effect}/enabled");

        const auto effectId = toLowerAscii (segments[0]);
        const auto* effect = registry_.findEffect (effectId);

        if (effect == nullptr)
            return jsonError (404, "Unknown effect");

        const auto parsed = parseBody (body);

        bool enabled = false;

        if (! readBool (parsed, "enabled", enabled))
            return jsonError (400, "Body must contain a boolean 'enabled'");

        if (! registry_.setEffectEnabled (effectId, enabled))
            return jsonError (503, "Effect cannot be toggled");

        auto* object = new juce::DynamicObject();
        object->setProperty ("effect", juce::String (effectId));
        object->setProperty ("enabled", effect->isEnabled ? effect->isEnabled() : enabled);

        return jsonOk (juce::var (object));
    }
    catch (const std::exception& e)
    {
        return jsonError (500, juce::String ("Exception: ") + e.what());
    }
}

HttpHandler::Response EffectsHandler::handlePut (const std::string& path, const std::string& body)
{
    try
    {
        const auto segments = pathSegmentsAfter (path, kPrefix);

        if (segments.size() != 2)
            return jsonError (404, "Expected /api/effects/{effect}/{parameter}");

        const auto effectId = toLowerAscii (segments[0]);
        const auto parsed = parseBody (body);

        // PUT .../enabled is accepted as well as POST, since the UI treats every
        // control the same way.
        if (segments[1] == "enabled")
        {
            bool enabled = false;

            if (! readBool (parsed, "enabled", enabled) && ! readBool (parsed, "value", enabled))
                return jsonError (400, "Body must contain a boolean 'enabled'");

            if (! registry_.setEffectEnabled (effectId, enabled))
                return jsonError (404, "Unknown effect");

            auto* object = new juce::DynamicObject();
            object->setProperty ("effect", juce::String (effectId));
            object->setProperty ("enabled", enabled);

            return jsonOk (juce::var (object));
        }

        double value = 0.0;

        if (! readNumber (parsed, "value", value))
            return jsonError (400, "Body must contain a numeric 'value'");

        float applied = 0.0f;

        if (! registry_.setParameter (effectId, segments[1], (float) value, applied))
            return jsonError (404, "Unknown effect or parameter");

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
