#include "api/ScenesHandler.h"

#include "api/ApiJson.h"

using namespace milodikfx::api;

namespace
{
constexpr const char* kPrefix = "/api/scenes";

/** Parses the slot number from the first path segment. -1 when it is not one. */
int parseIndex (const std::vector<std::string>& segments)
{
    if (segments.empty())
        return -1;

    const juce::String text (segments[0]);

    if (text.isEmpty() || ! text.containsOnly ("0123456789"))
        return -1;

    const auto index = text.getIntValue();

    return milodikfx::preset::SceneManager::isValidIndex (index) ? index : -1;
}
} // namespace

HttpHandler::Response ScenesHandler::describeState() const
{
    juce::Array<juce::var> array;

    for (int i = 0; i < milodikfx::preset::SceneManager::kNumScenes; ++i)
    {
        const auto& scene = manager.getScene (i);

        auto* object = new juce::DynamicObject();
        object->setProperty ("index", i);
        object->setProperty ("name", scene.name);
        object->setProperty ("populated", scene.populated);

        auto* flags = new juce::DynamicObject();

        for (const auto& [effectId, enabled] : scene.enabled)
            flags->setProperty (juce::Identifier (effectId), enabled);

        object->setProperty ("enabled", juce::var (flags));

        // Which channel each effect lands on, so the stage view can badge the
        // scene buttons with the letters. Empty when no channel store is attached.
        auto* channels = new juce::DynamicObject();

        for (const auto& [effectId, channelIndex] : scene.channels)
            channels->setProperty (juce::Identifier (effectId), channelIndex);

        object->setProperty ("channels", juce::var (channels));

        array.add (juce::var (object));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("scenes", array);

    // -1 once the chain has been changed by hand, so the UI can show that no
    // slot describes what you are hearing rather than highlighting a stale one.
    root->setProperty ("active", manager.getActiveIndex());

    return jsonOk (juce::var (root));
}

HttpHandler::Response ScenesHandler::handleGet (const std::string& path, const std::string&) const
{
    if (! pathSegmentsAfter (path, kPrefix).empty())
        return jsonError (404, "Unknown scene endpoint");

    return describeState();
}

HttpHandler::Response ScenesHandler::handlePost (const std::string& path, const std::string&)
{
    const auto segments = pathSegmentsAfter (path, kPrefix);
    const auto index = parseIndex (segments);

    if (index < 0 || segments.size() != 2)
        return jsonError (404, "Expected /api/scenes/<0-3>/recall or /capture");

    const auto action = toLowerAscii (segments[1]);

    if (action == "recall")
    {
        if (! manager.recall (index))
            return jsonError (409, "That scene is empty");
    }
    else if (action == "capture")
    {
        manager.capture (index);
    }
    else
    {
        return jsonError (404, "Unknown scene action");
    }

    if (onChanged)
        onChanged();

    return describeState();
}

HttpHandler::Response ScenesHandler::handlePut (const std::string& path, const std::string& body)
{
    const auto segments = pathSegmentsAfter (path, kPrefix);
    const auto index = parseIndex (segments);

    if (index < 0)
        return jsonError (404, "Expected /api/scenes/<0-3>");

    const auto parsed = parseBody (body);

    if (! parsed.isObject())
        return jsonError (400, "Body must be a JSON object");

    if (segments.size() == 1)
    {
        const auto name = parsed["name"].toString();

        if (! manager.setName (index, name))
            return jsonError (400, "A scene needs a name");
    }
    else if (segments.size() == 3 && toLowerAscii (segments[1]) == "effects")
    {
        bool enabled = false;

        if (! readBool (parsed, "enabled", enabled))
            return jsonError (400, "Expected {\"enabled\": true|false}");

        const auto effectId = juce::URL::removeEscapeChars (juce::String (segments[2]));

        if (! manager.setEffectEnabled (index, effectId, enabled))
            return jsonError (404, "No such effect, or it is always in the path");
    }
    else
    {
        return jsonError (404, "Unknown scene endpoint");
    }

    if (onChanged)
        onChanged();

    return describeState();
}
