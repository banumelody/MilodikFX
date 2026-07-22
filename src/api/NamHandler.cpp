#include "api/NamHandler.h"

#include "api/ApiJson.h"

using namespace milodikfx::api;

namespace
{
constexpr const char* kPrefix = "/api/nam";

// A .nam is JSON text weights: a Standard model is ~285 KB, a large one a few
// MB. 32 MB is well past anything real and still bounds the request.
constexpr int kMaxNamBytes = 32 * 1024 * 1024;

juce::Array<juce::var> toVarArray (const juce::StringArray& names)
{
    juce::Array<juce::var> array;

    for (const auto& name : names)
        array.add (name);

    return array;
}
} // namespace

HttpHandler::Response NamHandler::handleGet (const std::string&, const std::string&) const
{
    try
    {
        auto* root = new juce::DynamicObject();

        root->setProperty ("models", toVarArray (library.list()));
        root->setProperty ("directory", library.getDirectory().getFullPathName());

        // Whether models can actually be loaded here, and if not, why. The UI
        // shows the reason rather than letting a load silently fail.
        root->setProperty ("available", milodikfx::dsp::NamProcessor::isAvailable());
        root->setProperty ("unavailableReason", milodikfx::dsp::NamProcessor::unavailableReason());

        return jsonOk (juce::var (root));
    }
    catch (const std::exception& e)
    {
        return jsonError (500, juce::String ("Exception: ") + e.what());
    }
}

HttpHandler::Response NamHandler::handlePost (const std::string& path, const std::string& body)
{
    try
    {
        const auto segments = pathSegmentsAfter (path, kPrefix);
        const auto action = segments.empty() ? std::string ("import") : toLowerAscii (segments[0]);

        if (action == "reveal")
        {
            const auto directory = library.getDirectory();
            directory.createDirectory();
            directory.revealToUser();

            auto* object = new juce::DynamicObject();
            object->setProperty ("revealed", directory.getFullPathName());

            return jsonOk (juce::var (object));
        }

        if (action == "import")
        {
            const auto parsed = parseBody (body);

            if (! parsed.isObject())
                return jsonError (400, "Body must be a JSON object");

            const auto name = parsed["name"].toString();

            if (name.trim().isEmpty())
                return jsonError (400, "Missing 'name'");

            const auto encoded = parsed["data"].toString();

            if (encoded.isEmpty())
                return jsonError (400, "Missing base64 'data'");

            juce::MemoryOutputStream decoded;

            if (! juce::Base64::convertFromBase64 (decoded, encoded))
                return jsonError (400, "'data' is not valid base64");

            if (decoded.getDataSize() == 0)
                return jsonError (400, "Decoded file is empty");

            if ((int) decoded.getDataSize() > kMaxNamBytes)
                return jsonError (413, "Model file is too large");

            juce::MemoryBlock block (decoded.getData(), decoded.getDataSize());

            const auto stored = library.import (name, block);

            if (stored.isEmpty())
                return jsonError (400, "Could not store the model");

            if (onLibraryChanged)
                onLibraryChanged();

            auto* object = new juce::DynamicObject();
            object->setProperty ("name", stored);
            object->setProperty ("models", toVarArray (library.list()));

            return jsonOk (juce::var (object));
        }

        return jsonError (404, "Unknown NAM action");
    }
    catch (const std::exception& e)
    {
        return jsonError (500, juce::String ("Exception: ") + e.what());
    }
}
