#pragma once

#include "api/HttpHandler.h"
#include "dsp/TunerAnalyzer.h"

/**
 * GET  /api/tuner        - the current reading
 * POST /api/tuner/enable - {"enabled": bool}
 * SSE  /api/tuner/stream - the reading ~16 times a second while analysis is on
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

    /**
     * The SSE payload: a compact reading while the tuner is on, empty while it
     * is off. Returning empty means the stream sends nothing at all when nobody
     * is tuning, so an idle open panel costs one atomic read per tick.
     */
    std::string streamPayload() const;

private:
    milodikfx::dsp::TunerAnalyzer& analyzer;
};
