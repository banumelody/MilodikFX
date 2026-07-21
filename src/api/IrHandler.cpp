#include "api/IrHandler.h"

#include "api/ApiJson.h"

using namespace milodikfx::api;
using Category = milodikfx::preset::IrLibrary::Category;

namespace
{
constexpr const char* kPrefix = "/api/ir";

// A single impulse response is small; anything past this is not one.
constexpr int kMaxIrBytes = 16 * 1024 * 1024;

juce::Array<juce::var> toVarArray (const juce::StringArray& names)
{
    juce::Array<juce::var> array;

    for (const auto& name : names)
        array.add (name);

    return array;
}

Category categoryFromString (const juce::String& text)
{
    return text.equalsIgnoreCase ("reverb") ? Category::reverb : Category::cabinet;
}
} // namespace

HttpHandler::Response IrHandler::handleGet (const std::string&, const std::string&) const
{
    try
    {
        auto* root = new juce::DynamicObject();

        root->setProperty ("cabinets", toVarArray (library.list (Category::cabinet)));
        root->setProperty ("reverbs", toVarArray (library.list (Category::reverb)));
        root->setProperty ("cabinetDirectory", library.getDirectory (Category::cabinet).getFullPathName());
        root->setProperty ("reverbDirectory", library.getDirectory (Category::reverb).getFullPathName());

        return jsonOk (juce::var (root));
    }
    catch (const std::exception& e)
    {
        return jsonError (500, juce::String ("Exception: ") + e.what());
    }
}

HttpHandler::Response IrHandler::handlePost (const std::string& path, const std::string& body)
{
    try
    {
        const auto segments = pathSegmentsAfter (path, kPrefix);
        const auto action = segments.empty() ? std::string ("import") : toLowerAscii (segments[0]);

        const auto parsed = parseBody (body);

        if (! parsed.isObject())
            return jsonError (400, "Body must be a JSON object");

        const auto category = categoryFromString (parsed["category"].toString());

        if (action == "reveal")
        {
            // Opening Explorer is the reliable way to get files into the folder;
            // relying on WebView2's download behaviour would be a guess.
            const auto directory = library.getDirectory (category);
            directory.createDirectory();
            directory.revealToUser();

            auto* object = new juce::DynamicObject();
            object->setProperty ("revealed", directory.getFullPathName());

            return jsonOk (juce::var (object));
        }

        if (action == "import")
        {
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

            if ((int) decoded.getDataSize() > kMaxIrBytes)
                return jsonError (413, "Impulse response is too large");

            juce::MemoryBlock block (decoded.getData(), decoded.getDataSize());

            const auto stored = library.import (category, name, block);

            if (stored.isEmpty())
                return jsonError (400, "Could not store the impulse response");

            if (onLibraryChanged)
                onLibraryChanged();

            auto* object = new juce::DynamicObject();
            object->setProperty ("name", stored);
            object->setProperty ("cabinets", toVarArray (library.list (Category::cabinet)));
            object->setProperty ("reverbs", toVarArray (library.list (Category::reverb)));

            return jsonOk (juce::var (object));
        }

        return jsonError (404, "Unknown impulse-response action");
    }
    catch (const std::exception& e)
    {
        return jsonError (500, juce::String ("Exception: ") + e.what());
    }
}
