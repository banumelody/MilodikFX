#pragma once

#include "HttpHandler.h"
#include <JuceHeader.h>

/**
 * Handles /api/presets/* endpoints for preset management.
 * GET /api/presets - List available presets
 * POST /api/presets/save - Save current state as preset
 * POST /api/presets/load - Load a preset
 */
class PresetsHandler final : public HttpHandler
{
public:
    explicit PresetsHandler(juce::File presetsDirectory)
        : presetsDir_(presetsDirectory)
    {
        if (!presetsDir_.exists())
            presetsDir_.createDirectory();
    }

    Response handleGet(const std::string& path, const std::string& query) const override;
    Response handlePost(const std::string& path, const std::string& body) override;

private:
    juce::File presetsDir_;
};
