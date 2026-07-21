#pragma once

#include <JuceHeader.h>

#include "api/HttpHandler.h"
#include "audio/AudioDeviceController.h"

/**
 * /api/devices
 *
 *   GET  /api/devices          current device plus everything available
 *   POST /api/devices          { type?, inputDevice?, outputDevice?, sampleRate?, bufferSize? }
 *   POST /api/devices/select   legacy alias for the above
 *
 * All work is delegated to AudioDeviceController, which marshals onto the
 * message thread -- this handler runs on a Winsock connection thread and must
 * never touch the device manager itself.
 */
class DevicesHandler final : public HttpHandler
{
public:
    explicit DevicesHandler (milodikfx::audio::AudioDeviceController& controllerToUse)
        : controller (controllerToUse)
    {
    }

    Response handleGet (const std::string& path, const std::string& query) const override;
    Response handlePost (const std::string& path, const std::string& body) override;

private:
    milodikfx::audio::AudioDeviceController& controller;
};
