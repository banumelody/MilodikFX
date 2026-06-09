#pragma once

#include "HttpHandler.h"
#include <JuceHeader.h>
#include "../audio/AudioEngine.h"

/**
 * Handles /api/parameters/* endpoints for DSP parameter control.
 * GET: Returns current parameter values
 * PUT: Sets parameter values
 */
class ParametersHandler final : public HttpHandler
{
public:
    explicit ParametersHandler(
        milodikfx::audio::AudioEngine& engine,
        juce::PropertiesFile& settings)
        : engine_(engine), settings_(settings)
    {
    }

    Response handleGet(const std::string& path, const std::string& query) const override;
    Response handlePut(const std::string& path, const std::string& body) override;

private:
    milodikfx::audio::AudioEngine& engine_;
    juce::PropertiesFile& settings_;
};
