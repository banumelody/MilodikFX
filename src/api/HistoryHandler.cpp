#include "api/HistoryHandler.h"

#include "api/ApiJson.h"

using namespace milodikfx::api;

HttpHandler::Response HistoryHandler::describeState (bool includeEffects) const
{
    auto* root = new juce::DynamicObject();

    root->setProperty ("canUndo", history.canUndo());
    root->setProperty ("canRedo", history.canRedo());
    root->setProperty ("undoDepth", history.getUndoDepth());
    root->setProperty ("redoDepth", history.getRedoDepth());

    if (includeEffects)
        root->setProperty ("effects", registry.toVar()["effects"]);

    return jsonOk (juce::var (root));
}

HttpHandler::Response HistoryHandler::handleGet (const std::string& path, const std::string&) const
{
    if (! pathSegmentsAfter (path, "/api/history").empty())
        return jsonError (404, "Unknown history endpoint");

    return describeState (false);
}

HttpHandler::Response HistoryHandler::handlePost (const std::string& path, const std::string&)
{
    const auto segments = pathSegmentsAfter (path, "/api/history");

    if (segments.size() != 1)
        return jsonError (404, "Expected /api/history/undo or /redo");

    const auto action = toLowerAscii (segments[0]);

    auto moved = false;

    if (action == "undo")
        moved = history.undo();
    else if (action == "redo")
        moved = history.redo();
    else
        return jsonError (404, "Unknown history action");

    // 409 rather than an error: asking to undo with nothing to undo is a
    // perfectly ordinary thing for a keyboard shortcut to do.
    if (! moved)
        return jsonResponse (409, [this, action]
        {
            auto* root = new juce::DynamicObject();
            root->setProperty ("error", action == "undo" ? "Tidak ada yang bisa dibatalkan"
                                                         : "Tidak ada yang bisa diulang");
            root->setProperty ("canUndo", history.canUndo());
            root->setProperty ("canRedo", history.canRedo());
            return juce::var (root);
        }());

    if (onChanged)
        onChanged();

    return describeState (true);
}
