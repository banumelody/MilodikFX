#pragma once

#include "api/HttpHandler.h"
#include "midi/MidiController.h"

/**
 * GET    /api/midi                 - devices, what is open, and every mapping
 * POST   /api/midi/device          - {"name": "..."}; "" closes
 * PUT    /api/midi/mappings/<cc>   - {"effect": "...", "parameter": "...", "mode": "toggle"|"continuous"}
 * DELETE /api/midi/mappings/<cc>   - unbind
 * POST   /api/midi/learn           - {"effect": "...", "parameter": "..."} to arm, {} to disarm
 */
class MidiHandler final : public HttpHandler
{
public:
    explicit MidiHandler (milodikfx::midi::MidiController& controllerToUse) : controller (controllerToUse) {}

    /**
     * Fired with the device the user chose (empty when they pick "none"). The app
     * remembers this as the *desired* device and keeps trying to reopen it, so a
     * wireless pedal that went to sleep reconnects on its own.
     */
    std::function<void (const juce::String&)> onDeviceChosen;

    Response handleGet (const std::string& path, const std::string& query) const override;
    Response handlePost (const std::string& path, const std::string& body) override;
    Response handlePut (const std::string& path, const std::string& body) override;
    Response handleDelete (const std::string& path) override;

private:
    Response describeState() const;

    milodikfx::midi::MidiController& controller;
};
