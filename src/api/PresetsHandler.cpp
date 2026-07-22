#include "api/PresetsHandler.h"

#include "api/ApiJson.h"

using namespace milodikfx::api;

namespace
{
constexpr const char* kPrefix = "/api/presets";

juce::var metadataVar (const juce::String& name, const milodikfx::preset::PresetMetadata& metadata)
{
    auto* object = new juce::DynamicObject();

    object->setProperty ("name", name);
    object->setProperty ("description", metadata.description);
    object->setProperty ("favourite", metadata.favourite);
    object->setProperty ("notes", metadata.notes);
    object->setProperty ("savedAt", metadata.savedAt);

    juce::Array<juce::var> tags;

    for (const auto& tag : metadata.tags)
        tags.add (tag);

    object->setProperty ("tags", tags);

    return juce::var (object);
}

juce::var presetListVar (const milodikfx::preset::PresetManager& manager, const juce::String& selected)
{
    const auto names = manager.listPresets();

    // Names on their own, as well as the detailed list. Several callers -- the
    // E2E suite among them -- only ever want to know whether a name is there,
    // and dropping the flat array would have broken all of them.
    juce::Array<juce::var> flat;
    juce::Array<juce::var> detailed;

    for (const auto& name : names)
    {
        flat.add (name);

        milodikfx::preset::PresetDocument document;
        manager.loadDocument (name, document);

        detailed.add (metadataVar (name, document.metadata));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("presets", flat);
    root->setProperty ("details", detailed);
    root->setProperty ("selected", selected);

    return juce::var (root);
}
} // namespace

HttpHandler::Response PresetsHandler::handleGet (const std::string&, const std::string&) const
{
    try
    {
        return jsonOk (presetListVar (presetManager, selectedName));
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

    return jsonOk (presetListVar (presetManager, selectedName));
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
            milodikfx::preset::PresetDocument document;

            // Keeps the metadata the file already had; only the sound and the
            // scenes are being replaced.
            presetManager.loadDocument (name, document);

            // Fold the live edits into each effect's active channel first, so
            // the stored channel matches what the state snapshot is about to
            // capture -- otherwise reloading and returning to the active channel
            // would revert the edits made since the last switch.
            if (channelStore != nullptr)
                channelStore->commitActive();

            document.state = registry.captureState();

            if (sceneManager != nullptr)
                document.scenes = sceneManager->toVar();

            if (channelStore != nullptr)
                document.channels = channelStore->toVar();

            if (! presetManager.saveDocument (name, document))
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
            milodikfx::preset::PresetDocument document;

            if (! presetManager.loadDocument (name, document))
                return jsonError (404, "Preset not found");

            const auto applied = registry.applyState (document.state);

            if (sceneManager != nullptr)
            {
                // A version 2 file has no scenes. Rather than leave the slots
                // holding the previous preset's pattern -- which would recall
                // something that has nothing to do with what is loaded -- fill
                // them from the chain the preset just put in place.
                if (document.scenes.isArray())
                    sceneManager->fromVar (document.scenes);
                else
                    sceneManager->resetToCurrent();
            }

            if (channelStore != nullptr)
            {
                // The state has already been applied above, so an older preset
                // with no channels seeds all four from the sound it just put in
                // place -- never from the previous preset's channels.
                if (document.channels.isObject())
                    channelStore->fromVar (document.channels);
                else
                    channelStore->resetToCurrent();
            }

            selectedName = name;

            if (onSelectionChanged)
                onSelectionChanged (selectedName);

            auto* root = new juce::DynamicObject();
            root->setProperty ("success", true);
            root->setProperty ("name", name);
            root->setProperty ("valuesApplied", applied);
            root->setProperty ("metadata", metadataVar (name, document.metadata));
            root->setProperty ("effects", registry.toVar()["effects"]);

            return jsonOk (juce::var (root));
        }

        if (action == "delete")
            return deleteByName (name);

        if (action == "metadata")
        {
            milodikfx::preset::PresetDocument document;

            if (! presetManager.loadDocument (name, document))
                return jsonError (404, "Preset not found");

            auto metadata = document.metadata;

            // Only the fields actually present are touched, so a UI that edits
            // the notes alone cannot wipe the tags.
            if (! parsed["description"].isVoid())
                metadata.description = parsed["description"].toString();

            if (! parsed["notes"].isVoid())
                metadata.notes = parsed["notes"].toString();

            if (! parsed["favourite"].isVoid())
                metadata.favourite = (bool) parsed["favourite"];

            if (const auto* tags = parsed["tags"].getArray())
            {
                metadata.tags.clear();

                for (const auto& tag : *tags)
                    if (const auto text = tag.toString().trim(); text.isNotEmpty())
                        metadata.tags.addIfNotAlreadyThere (text);
            }

            if (! presetManager.updateMetadata (name, metadata))
                return jsonError (500, "Failed to write the preset file");

            return jsonOk (metadataVar (name, metadata));
        }

        if (action == "export")
        {
            const auto text = presetManager.exportPreset (name);

            if (text.isEmpty())
                return jsonError (404, "Preset not found");

            auto* root = new juce::DynamicObject();
            root->setProperty ("name", name);
            root->setProperty ("filename", name + ".milodikfx.json");
            root->setProperty ("data", text);

            return jsonOk (juce::var (root));
        }

        if (action == "import")
        {
            const auto data = parsed["data"].toString();

            if (data.isEmpty())
                return jsonError (400, "Missing 'data'");

            const auto stored = presetManager.importPreset (name, data);

            if (stored.isEmpty())
                return jsonError (400, "That file is not a MilodikFX preset");

            return jsonOk (presetListVar (presetManager, selectedName));
        }

        return jsonError (404, "Unknown preset action");
    }
    catch (const std::exception& e)
    {
        return jsonError (500, juce::String ("Exception: ") + e.what());
    }
}
