#pragma once

#include "HttpHandler.h"
#include <JuceHeader.h>

/**
 * Handles /api/devices endpoint for device enumeration and selection.
 * GET: Returns list of available input/output devices
 * POST: Sets selected input/output devices
 */
class DevicesHandler final : public HttpHandler
{
public:
    explicit DevicesHandler(juce::AudioDeviceManager& deviceManager)
        : deviceManager_(deviceManager)
    {
    }

    Response handleGet(const std::string& path, const std::string& query) const override;
    Response handlePost(const std::string& path, const std::string& body) override;

private:
    juce::AudioDeviceManager& deviceManager_;
};
