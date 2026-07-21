#pragma once

#include <JuceHeader.h>

#include <functional>

#include "api/HttpHandler.h"
#include "api/ParameterRegistry.h"
#include "preset/PresetManager.h"
#include "preset/SceneManager.h"

/**
 * /api/presets
 *
 *   GET    /api/presets              { "presets": [ {name, description, tags, favourite, ...} ], "selected": "..." }
 *   POST   /api/presets/save         { "name": "..." }
 *   POST   /api/presets/load         { "name": "..." }
 *   POST   /api/presets/delete       { "name": "..." }
 *   POST   /api/presets/metadata     { "name": "...", "description", "tags", "favourite", "notes" }
 *   POST   /api/presets/export       { "name": "..." } -> the file's text
 *   POST   /api/presets/import       { "name": "...", "data": "<json>" }
 *   DELETE /api/presets/{name}
 *
 * The saved payload comes straight from the parameter registry, so every
 * registered control is captured without this class knowing what they are.
 * Metadata sits beside it rather than inside it -- see PresetMetadata.
 */
class PresetsHandler final : public HttpHandler
{
public:
    PresetsHandler (milodikfx::preset::PresetManager& presetManagerToUse,
                    const milodikfx::api::ParameterRegistry& registryToUse)
        : presetManager (presetManagerToUse),
          registry (registryToUse)
    {
    }

    /**
     * Scenes travel inside the preset file, so saving and loading one has to
     * carry them. Optional: a plugin build has no scene manager.
     */
    void setSceneManager (milodikfx::preset::SceneManager* manager) { sceneManager = manager; }

    Response handleGet (const std::string& path, const std::string& query) const override;
    Response handlePost (const std::string& path, const std::string& body) override;
    Response handleDelete (const std::string& path) override;

    /** Notified with the preset name whenever one is loaded or saved. */
    std::function<void (const juce::String&)> onSelectionChanged;

    void setSelectedName (const juce::String& name) { selectedName = name; }
    juce::String getSelectedName() const { return selectedName; }

private:
    Response deleteByName (const juce::String& name);

    milodikfx::preset::PresetManager& presetManager;
    const milodikfx::api::ParameterRegistry& registry;
    milodikfx::preset::SceneManager* sceneManager = nullptr;
    juce::String selectedName;
};
