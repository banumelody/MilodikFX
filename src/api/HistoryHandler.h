#pragma once

#include "api/HttpHandler.h"
#include "api/UndoHistory.h"

/**
 * GET  /api/history       - what is available to undo or redo
 * POST /api/history/undo
 * POST /api/history/redo
 *
 * Both actions answer with the effects list as well, so the UI redraws from
 * what the engine actually settled on rather than guessing.
 */
class HistoryHandler final : public HttpHandler
{
public:
    HistoryHandler (milodikfx::api::UndoHistory& historyToUse,
                    const milodikfx::api::ParameterRegistry& registryToUse)
        : history (historyToUse),
          registry (registryToUse)
    {
    }

    Response handleGet (const std::string& path, const std::string& query) const override;
    Response handlePost (const std::string& path, const std::string& body) override;

    /** Fired after an undo or redo actually changed something. */
    std::function<void()> onChanged;

private:
    Response describeState (bool includeEffects) const;

    milodikfx::api::UndoHistory& history;
    const milodikfx::api::ParameterRegistry& registry;
};
