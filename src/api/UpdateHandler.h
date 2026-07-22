#pragma once

#include <JuceHeader.h>

#include <array>
#include <mutex>
#include <string>

#include "api/ApiJson.h"
#include "api/HttpHandler.h"

// The shipping version is baked in at configure time so the check has something
// to compare against. The fallback keeps the header compilable in the test
// binary, which does not define it.
#ifndef MILODIKFX_VERSION
 #define MILODIKFX_VERSION "0.0.0"
#endif

namespace milodikfx::api
{
/** Parses "v0.15.0" (or "0.15.0", or "0.15") into {major, minor, patch}. */
inline std::array<int, 3> parseSemver (juce::String s)
{
    s = s.trim();

    if (s.startsWithChar ('v') || s.startsWithChar ('V'))
        s = s.substring (1);

    std::array<int, 3> out { 0, 0, 0 };
    int index = 0;

    for (const auto& part : juce::StringArray::fromTokens (s, ".", ""))
    {
        if (index >= 3)
            break;

        // getIntValue reads the leading digits and stops, so a "-rc1" suffix or
        // any trailing text on a component becomes just its number.
        out[(size_t) index++] = part.getIntValue();
    }

    return out;
}

/** True when `latest` is a strictly higher version than `current`. */
inline bool isNewerVersion (const juce::String& current, const juce::String& latest)
{
    const auto c = parseSemver (current);
    const auto l = parseSemver (latest);

    for (int i = 0; i < 3; ++i)
        if (l[(size_t) i] != c[(size_t) i])
            return l[(size_t) i] > c[(size_t) i];

    return false;
}
} // namespace milodikfx::api

/**
 * Checks GitHub Releases for a newer build and reports it to the UI, which
 * raises a dismissible banner. The current version is compiled in
 * (MILODIKFX_VERSION); the latest is read from the public releases API.
 *
 * The network read runs on the calling Winsock worker thread -- never the audio
 * thread -- and carries a short connection timeout so a blocked network cannot
 * hold the connection open. A successful result is cached for a while, so
 * reloading the UI (or a page stuck in a reload loop) cannot hammer the API past
 * its unauthenticated per-IP rate limit. A failed lookup is not cached, so a
 * transient outage is retried on the next request rather than remembered.
 *
 * What the check itself really does -- that a given release exists on GitHub --
 * needs the network, so it is exercised by hand; the version comparison it turns
 * on is a pure function, `isNewerVersion`, and that is unit-tested.
 */
class UpdateHandler final : public HttpHandler
{
public:
    UpdateHandler() = default;
    ~UpdateHandler() override = default;

    Response handleGet (const std::string& /*path*/, const std::string& query) const override
    {
        const bool force = query.find ("force") != std::string::npos;

        std::lock_guard<std::mutex> lock (mutex);

        const auto now = juce::Time::getMillisecondCounter();

        if (! force && haveCache && (now - cachedAtMs) < kCacheTtlMs)
            return milodikfx::api::jsonOk (cached);

        bool ok = false;
        auto result = fetchLatest (ok);

        if (ok)
        {
            cached = result;
            cachedAtMs = now;
            haveCache = true;
        }

        return milodikfx::api::jsonOk (result);
    }

private:
    static constexpr juce::uint32 kCacheTtlMs = 30 * 60 * 1000;

    juce::var fetchLatest (bool& ok) const
    {
        ok = false;

        const juce::String current (MILODIKFX_VERSION);

        auto* object = new juce::DynamicObject();
        object->setProperty ("current", current);
        object->setProperty ("latest", juce::String());
        object->setProperty ("updateAvailable", false);
        object->setProperty ("url", juce::String());
        object->setProperty ("name", juce::String());

        juce::URL url ("https://api.github.com/repos/banumelody/MilodikFX/releases/latest");

        // GitHub rejects a request with no User-Agent outright, so it is not
        // optional. The connection timeout bounds the worst case.
        const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                 .withExtraHeaders ("User-Agent: MilodikFX\r\nAccept: application/vnd.github+json")
                                 .withConnectionTimeoutMs (5000);

        std::unique_ptr<juce::InputStream> stream (url.createInputStream (options));

        if (stream == nullptr)
        {
            object->setProperty ("error", "Tidak dapat menghubungi GitHub");
            return juce::var (object);
        }

        const auto text = stream->readEntireStreamAsString();

        juce::var parsed;

        if (juce::JSON::parse (text, parsed).failed() || ! parsed.isObject())
        {
            object->setProperty ("error", "Respons GitHub tidak dapat dibaca");
            return juce::var (object);
        }

        const auto tag = parsed.getProperty ("tag_name", {}).toString();

        if (tag.isEmpty())
        {
            object->setProperty ("error", "Rilis terbaru tidak ditemukan");
            return juce::var (object);
        }

        object->setProperty ("latest", tag);
        object->setProperty ("url", parsed.getProperty ("html_url", {}).toString());
        object->setProperty ("name", parsed.getProperty ("name", {}).toString());
        object->setProperty ("updateAvailable", milodikfx::api::isNewerVersion (current, tag));

        ok = true;
        return juce::var (object);
    }

    mutable std::mutex mutex;
    mutable bool haveCache = false;
    mutable juce::uint32 cachedAtMs = 0;
    mutable juce::var cached;
};
