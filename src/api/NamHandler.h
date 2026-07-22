#pragma once

#include <JuceHeader.h>

#include <functional>

#include "api/HttpHandler.h"
#include "dsp/NamProcessor.h"
#include "preset/NamLibrary.h"

/**
 * /api/nam
 *
 *   GET  /api/nam           the library, plus whether this build can run models
 *   POST /api/nam/import    { name, data }  data is base64
 *   POST /api/nam/reveal    opens the models folder in Explorer
 *
 * Choosing which model an effect uses is not here -- that is a registry
 * parameter reached through /api/effects. This handler manages the files on
 * disk and reports availability (a build without NAM, or a CPU without AVX2,
 * still answers, so the UI can explain why loading is disabled).
 */
class NamHandler final : public HttpHandler
{
public:
    explicit NamHandler (milodikfx::preset::NamLibrary& libraryToUse)
        : library (libraryToUse)
    {
    }

    Response handleGet (const std::string& path, const std::string& query) const override;
    Response handlePost (const std::string& path, const std::string& body) override;

    /** Notified when the set of files on disk changed. */
    std::function<void()> onLibraryChanged;

private:
    milodikfx::preset::NamLibrary& library;
};
