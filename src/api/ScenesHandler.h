#pragma once

#include "api/HttpHandler.h"
#include "preset/SceneManager.h"

/**
 * GET  /api/scenes                        - the four slots and which is active
 * POST /api/scenes/<n>/recall             - jump to a slot
 * POST /api/scenes/<n>/capture            - store the chain's pattern into it
 * PUT  /api/scenes/<n>                    - {"name": "..."}
 * PUT  /api/scenes/<n>/effects/<effectId> - {"enabled": bool}, edits the slot
 *                                            without touching the live chain
 */
class ScenesHandler final : public HttpHandler
{
public:
    explicit ScenesHandler (milodikfx::preset::SceneManager& managerToUse) : manager (managerToUse) {}

    Response handleGet (const std::string& path, const std::string& query) const override;
    Response handlePost (const std::string& path, const std::string& body) override;
    Response handlePut (const std::string& path, const std::string& body) override;

    /** Fired after anything that should end up in the settings or a preset. */
    std::function<void()> onChanged;

private:
    Response describeState() const;

    milodikfx::preset::SceneManager& manager;
};
