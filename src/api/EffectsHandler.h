#pragma once

#include <JuceHeader.h>

#include "api/HttpHandler.h"
#include "api/ParameterRegistry.h"

/**
 * /api/effects/*
 *
 *   GET  /api/effects                       every effect, in chain order
 *   GET  /api/effects/{effect}              one effect with its parameters
 *   POST /api/effects/{effect}/enabled      { "enabled": bool }
 *   PUT  /api/effects/{effect}/{param}      { "value": x }
 */
class EffectsHandler final : public HttpHandler
{
public:
    explicit EffectsHandler (const milodikfx::api::ParameterRegistry& registry)
        : registry_ (registry)
    {
    }

    Response handleGet (const std::string& path, const std::string& query) const override;
    Response handlePost (const std::string& path, const std::string& body) override;
    Response handlePut (const std::string& path, const std::string& body) override;

private:
    const milodikfx::api::ParameterRegistry& registry_;
};
