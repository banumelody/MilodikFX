#pragma once

#include "api/HttpHandler.h"
#include "dsp/TunerAnalyzer.h"

/**
 * GET  /api/tuner        - the current reading
 * POST /api/tuner/enable - {"enabled": bool}
 *
 * Analysis is off until asked for. It costs a background thread's worth of work
 * every 100 ms and there is no reason to pay that while nobody is tuning.
 */
class TunerHandler final : public HttpHandler
{
public:
    explicit TunerHandler (milodikfx::dsp::TunerAnalyzer& analyzerToUse) : analyzer (analyzerToUse) {}

    Response handleGet (const std::string& path, const std::string& query) const override;
    Response handlePost (const std::string& path, const std::string& body) override;

private:
    milodikfx::dsp::TunerAnalyzer& analyzer;
};
