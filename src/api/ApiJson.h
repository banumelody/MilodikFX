#pragma once

#include <JuceHeader.h>

#include <string>

#include "api/HttpHandler.h"

namespace milodikfx::api
{
/**
 * Response helpers built on juce::var / juce::JSON.
 *
 * Everything user-supplied (preset names, device names, error text) goes through
 * these, so a quote or backslash in a name can no longer break the response the
 * way hand-concatenated JSON did.
 */
inline std::string toJsonString (const juce::var& value)
{
    return juce::JSON::toString (value, false).toStdString();
}

inline HttpHandler::Response jsonResponse (int statusCode, const juce::var& value)
{
    return { statusCode, "application/json", toJsonString (value) };
}

inline HttpHandler::Response jsonError (int statusCode, const juce::String& message)
{
    auto* object = new juce::DynamicObject();
    object->setProperty ("error", message);

    return jsonResponse (statusCode, juce::var (object));
}

inline HttpHandler::Response jsonOk (const juce::var& value)
{
    return jsonResponse (200, value);
}

/** Parses a request body, returning a void var when it is not valid JSON. */
inline juce::var parseBody (const std::string& body)
{
    juce::var parsed;

    if (juce::JSON::parse (juce::String (body), parsed).wasOk())
        return parsed;

    return {};
}

/**
 * Reads a numeric field. Accepts JSON numbers and numeric strings, but rejects
 * booleans, nulls and anything non-finite.
 */
inline bool readNumber (const juce::var& source, const juce::Identifier& key, double& out)
{
    if (! source.isObject())
        return false;

    const auto value = source[key];

    if (value.isVoid() || value.isBool())
        return false;

    if (value.isDouble() || value.isInt() || value.isInt64())
    {
        out = (double) value;
        return std::isfinite (out);
    }

    if (value.isString())
    {
        const auto text = value.toString().trim();

        if (text.isEmpty() || ! text.containsOnly ("0123456789+-.eE"))
            return false;

        out = text.getDoubleValue();
        return std::isfinite (out);
    }

    return false;
}

inline bool readBool (const juce::var& source, const juce::Identifier& key, bool& out)
{
    if (! source.isObject())
        return false;

    const auto value = source[key];

    if (value.isBool() || value.isInt() || value.isInt64() || value.isDouble())
    {
        out = (bool) value;
        return true;
    }

    if (value.isString())
    {
        const auto text = value.toString().trim().toLowerCase();

        if (text == "true" || text == "1")
        {
            out = true;
            return true;
        }

        if (text == "false" || text == "0")
        {
            out = false;
            return true;
        }
    }

    return false;
}

/** Splits "/api/effects/overdrive/drive" into its segments after the prefix. */
inline std::vector<std::string> pathSegmentsAfter (const std::string& path, const std::string& prefix)
{
    std::vector<std::string> segments;

    if (path.rfind (prefix, 0) != 0)
        return segments;

    auto rest = path.substr (prefix.size());
    size_t start = 0;

    while (start < rest.size())
    {
        while (start < rest.size() && rest[start] == '/')
            ++start;

        if (start >= rest.size())
            break;

        const auto end = rest.find ('/', start);
        const auto length = end == std::string::npos ? std::string::npos : end - start;

        segments.push_back (rest.substr (start, length));

        if (end == std::string::npos)
            break;

        start = end + 1;
    }

    return segments;
}

inline std::string toLowerAscii (std::string s)
{
    for (auto& c : s)
        c = (char) juce::CharacterFunctions::toLowerCase (c);

    return s;
}
} // namespace milodikfx::api
