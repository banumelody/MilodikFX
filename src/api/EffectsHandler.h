#pragma once

#include "HttpHandler.h"
#include <JuceHeader.h>
#include "../dsp/DSPChainManager.h"

/**
 * Handles /api/effects/* endpoints for effect control.
 * POST /api/effects/{effect}/enabled - Toggle effect on/off
 * GET /api/effects/{effect} - Get effect parameters
 * PUT /api/effects/{effect}/{param} - Set effect parameter
 */
class EffectsHandler final : public HttpHandler
{
public:
    explicit EffectsHandler(milodikfx::dsp::DSPChainManager& chain)
        : chain_(chain)
    {
    }

    Response handleGet(const std::string& path, const std::string& query) const override;
    Response handlePost(const std::string& path, const std::string& body) override;
    Response handlePut(const std::string& path, const std::string& body) override;

private:
    milodikfx::dsp::DSPChainManager& chain_;
};
