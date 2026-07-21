#include "api/PresetsHandler.h"

#include "api/ApiJson.h"

using namespace milodikfx::api;

namespace
{
constexpr const char* kPrefix = "/api/presets";

juce::var presetListVar (const juce::StringArray& names, const juce::String& selected)
{
    juce::Array<juce::var> array;

    for (const auto& name : names)
        array.add (name);

    auto* root = new juce::DynamicObject();
    root->setProperty ("presets", array);
    root->setProperty ("selected", selected);

    return juce::var (root);
}
} // namespace

HttpHandler::Response PresetsHandler::handleGet (const std::string&, const std::string&) const
{
    try
    {
        return jsonOk (presetListVar (presetManager.listPresets(), selectedName));
    }
    catch (const std::exception& e)
    {
        return jsonError (500, juce::String ("Exception: ") + e.what());
    }
}

HttpHandler::Response PresetsHandler::deleteByName (const juce::String& name)
{
    if (! presetManager.deletePreset (name))
        return jsonError (404, "Preset not found");

    if (selectedName == milodikfx::preset::PresetManager::sanitisePresetName (name))
    {
        selectedName = {};

        if (onSelectionChanged)
            onSelectionChanged (selectedName);
    }

    return jsonOk (presetListVar (presetManager.listPresets(), selectedName));
}

HttpHandler::Response PresetsHandler::handleDelete (const std::string& path)
{
    try
    {
        const auto segments = pathSegmentsAfter (path, kPrefix);

        if (segments.size() != 1)
            return jsonError (404, "Expected /api/presets/{name}");

        return deleteByName (juce::URL::removeEscapeChars (juce::String (segments[0])));
    }
    catch (const std::exception& e)
    {
        return jsonError (500, juce::String ("Exception: ") + e.what());
    }
}

HttpHandler::Response PresetsHandler::handlePost (const std::string& path, const std::string& body)
{
    try
    {
        const auto parsed = parseBody (body);

        if (! parsed.isObject())
            return jsonError (400, "Body must be a JSON object");

        const auto rawName = parsed["name"].toString();
        const auto name = milodikfx::preset::PresetManager::sanitisePresetName (rawName);

        if (name.isEmpty())
            return jsonError (400, "Missing or invalid 'name'");

        const auto segments = pathSegmentsAfter (path, kPrefix);
        const auto action = segments.empty() ? std::string ("save") : toLowerAscii (segments[0]);

        if (action == "save")
        {
            if (! presetManager.savePreset (name, registry.captureState()))
                return jsonError (500, "Failed to write the preset file");

            selectedName = name;

            if (onSelectionChanged)
                onSelectionChanged (selectedName);

            auto* root = new juce::DynamicObject();
            root->setProperty ("success", true);
            root->setProperty ("name", name);

            return jsonOk (juce::var (root));
        }

        if (action == "load")
        {
            juce::var state;

            if (! presetManager.loadPreset (name, state))
                return jsonError (404, "Preset not found");

            const auto applied = registry.applyState (state);

            selectedName = name;

            if (onSelectionChanged)
                onSelectionChanged (selectedName);

            auto* root = new juce::DynamicObject();
            root->setProperty ("success", true);
            root->setProperty ("name", name);
            root->setProperty ("valuesApplied", applied);
            root->setProperty ("effects", registry.toVar()["effects"]);

            return jsonOk (juce::var (root));
        }

        if (action == "delete")
            return deleteByName (name);

        return jsonError (404, "Unknown preset action");
    }
    catch (const std::exception& e)
    {
        return jsonError (500, juce::String ("Exception: ") + e.what());
    }
}
