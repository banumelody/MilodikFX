#pragma once

#include <JuceHeader.h>

#include <functional>

#include "api/HttpHandler.h"
#include "preset/IrLibrary.h"

/**
 * /api/ir
 *
 *   GET  /api/ir           the library: folder paths and what is in them
 *   POST /api/ir/import    { category, name, data }  data is base64
 *   POST /api/ir/reveal    { category? }  opens the folder in Explorer
 *
 * Choosing which impulse response an effect uses is not here -- that is a
 * registry parameter like any other, reachable through /api/effects. This
 * handler only manages the files on disk.
 */
class IrHandler final : public HttpHandler
{
public:
    explicit IrHandler (milodikfx::preset::IrLibrary& libraryToUse)
        : library (libraryToUse)
    {
    }

    Response handleGet (const std::string& path, const std::string& query) const override;
    Response handlePost (const std::string& path, const std::string& body) override;

    /** Notified when the set of files on disk changed. */
    std::function<void()> onLibraryChanged;

private:
    milodikfx::preset::IrLibrary& library;
};
