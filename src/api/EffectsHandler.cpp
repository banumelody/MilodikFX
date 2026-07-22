#include "api/EffectsHandler.h"

#include "api/ApiJson.h"

using namespace milodikfx::api;

namespace
{
constexpr const char* kPrefix = "/api/effects";
} // namespace

void EffectsHandler::augment (juce::var& effectVar) const
{
    if (channelStore_ == nullptr)
        return;

    auto* object = effectVar.getDynamicObject();

    if (object == nullptr)
        return;

    const auto id = object->getProperty ("id").toString().toStdString();

    object->setProperty ("channel", channelStore_->getActive (id));

    juce::Array<juce::var> names;

    for (int i = 0; i < milodikfx::preset::ChannelStore::kNumChannels; ++i)
        names.add (channelStore_->getName (id, i));

    object->setProperty ("channels", names);
}

juce::var EffectsHandler::effectWithChannels (const milodikfx::api::EffectDescriptor& effect) const
{
    auto v = registry_.effectToVar (effect);
    augment (v);
    return v;
}

HttpHandler::Response EffectsHandler::handleGet (const std::string& path, const std::string&) const
{
    try
    {
        if (path == kPrefix || path == "/api/effects/")
        {
            if (channelStore_ == nullptr)
                return jsonOk (registry_.toVar());

            juce::Array<juce::var> effects;

            for (const auto& effect : registry_.getEffects())
                effects.add (effectWithChannels (effect));

            auto* root = new juce::DynamicObject();
            root->setProperty ("effects", effects);

            return jsonOk (juce::var (root));
        }

        const auto segments = pathSegmentsAfter (path, kPrefix);

        if (segments.empty())
            return jsonError (404, "Expected /api/effects/{effect}");

        const auto effectId = toLowerAscii (segments[0]);
        const auto* effect = registry_.findEffect (effectId);

        if (effect == nullptr)
            return jsonError (404, "Unknown effect");

        if (segments.size() == 1)
            return jsonOk (effectWithChannels (*effect));

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

            if (parameter == nullptr)
                return jsonError (404, "Unknown parameter");

            auto* object = new juce::DynamicObject();
            object->setProperty ("effect", juce::String (effectId));
            object->setProperty ("parameter", juce::String (parameter->id));

            if (parameter->isText)
                object->setProperty ("value", parameter->getText ? parameter->getText() : juce::String());
            else if (parameter->get)
                object->setProperty ("value", parameter->get());
            else
                return jsonError (404, "Unknown parameter");

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

        // Storing the live sound into one of the four channels.
        if (segments.size() == 3 && segments[1] == "channel" && segments[2] == "save")
        {
            if (channelStore_ == nullptr)
                return jsonError (503, "Channels are not available in this build");

            double index = 0.0;

            if (! readNumber (parsed, "value", index) && ! readNumber (parsed, "index", index))
                return jsonError (400, "Body must contain a numeric channel 'value' (0-3)");

            if (! channelStore_->save (effectId, (int) index))
                return jsonError (400, "Channel index out of range");

            return jsonOk (effectWithChannels (*effect));
        }

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

        // Selecting a channel: the whole block jumps to that saved sound. The
        // response carries the effect's new parameter values so the card can
        // redraw without a second round trip.
        if (segments[1] == "channel")
        {
            if (channelStore_ == nullptr)
                return jsonError (503, "Channels are not available in this build");

            double index = 0.0;

            if (! readNumber (parsed, "value", index) && ! readNumber (parsed, "index", index))
                return jsonError (400, "Body must contain a numeric channel 'value' (0-3)");

            if (! channelStore_->recall (effectId, (int) index))
                return jsonError (400, "Unknown effect or channel index out of range");

            const auto* effect = registry_.findEffect (effectId);

            return effect != nullptr ? jsonOk (effectWithChannels (*effect))
                                     : jsonError (404, "Unknown effect");
        }

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

        // Text-valued parameters (an impulse-response choice, say) carry a
        // string; everything else carries a number.
        const auto* descriptor = registry_.findParameter (effectId, segments[1]);

        if (descriptor != nullptr && descriptor->isText)
        {
            const auto requested = parsed["value"];

            if (! requested.isString())
                return jsonError (400, "Body must contain a string 'value'");

            juce::String appliedText;

            if (! registry_.setTextParameter (effectId, segments[1], requested.toString(), appliedText))
                return jsonError (404, "Unknown effect or parameter");

            auto* object = new juce::DynamicObject();
            object->setProperty ("effect", juce::String (effectId));
            object->setProperty ("parameter", juce::String (segments[1]));
            object->setProperty ("value", appliedText);

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
