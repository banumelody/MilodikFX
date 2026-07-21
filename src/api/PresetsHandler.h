#pragma once

#include <JuceHeader.h>

#include <functional>

#include "api/HttpHandler.h"
#include "api/ParameterRegistry.h"
#include "preset/PresetManager.h"

/**
 * /api/presets
 *
 *   GET    /api/presets              { "presets": [...], "selected": "..." }
 *   POST   /api/presets/save         { "name": "..." }
 *   POST   /api/presets/load         { "name": "..." }
 *   POST   /api/presets/delete       { "name": "..." }
 *   DELETE /api/presets/{name}
 *
 * The saved payload comes straight from the parameter registry, so every
 * registered control is captured without this class knowing what they are.
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
    juce::String selectedName;
};
