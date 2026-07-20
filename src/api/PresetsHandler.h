#pragma once

#include "HttpHandler.h"
#include <JuceHeader.h>
#include "../preset/PresetManager.h"
#include "../audio/AudioEngine.h"

/**
 * Handles /api/presets/* endpoints for preset management.
 * GET /api/presets - List available presets
 * POST /api/presets/save - Save current state as preset
 * POST /api/presets/load - Load a preset
 */
class PresetsHandler final : public HttpHandler
{
public:
    explicit PresetsHandler(milodikfx::preset::PresetManager& presetManager, milodikfx::audio::AudioEngine& engine)
        : presetManager_(presetManager), engine_(engine)
    {
    }

    Response handleGet(const std::string& path, const std::string& query) const override;
    Response handlePost(const std::string& path, const std::string& body) override;

private:
    milodikfx::preset::PresetManager& presetManager_;
    milodikfx::audio::AudioEngine& engine_;
};
