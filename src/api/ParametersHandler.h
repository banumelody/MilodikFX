#pragma once

#include <JuceHeader.h>

#include <functional>

#include "api/HttpHandler.h"
#include "api/ParameterRegistry.h"

/**
 * /api/parameters/*
 *
 *   GET  /api/parameters                        full state (same shape as /api/effects)
 *   GET  /api/parameters/master-volume          { "masterVolume": dB }
 *   PUT  /api/parameters/master-volume          { "value": dB }
 *   GET  /api/parameters/{effect}/{param}       { "value": x }
 *   PUT  /api/parameters/{effect}/{param}       { "value": x }
 *
 * Everything resolves through the registry, so an endpoint can never drift out
 * of sync with what the DSP chain actually exposes.
 */
class ParametersHandler final : public HttpHandler
{
public:
    ParametersHandler (const milodikfx::api::ParameterRegistry& registry,
                       std::string masterEffectId,
                       std::string masterParameterId)
        : registry_ (registry),
          masterEffectId_ (std::move (masterEffectId)),
          masterParameterId_ (std::move (masterParameterId))
    {
    }

    Response handleGet (const std::string& path, const std::string& query) const override;
    Response handlePut (const std::string& path, const std::string& body) override;

private:
    Response handleMasterVolumeGet() const;
    Response handleMasterVolumePut (const juce::var& body) const;

    const milodikfx::api::ParameterRegistry& registry_;
    std::string masterEffectId_;
    std::string masterParameterId_;
};
