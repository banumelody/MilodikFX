#pragma once

#include "api/ApiJson.h"
#include "api/HttpHandler.h"
#include "dsp/LooperProcessor.h"

/**
 * /api/looper
 *
 *   GET  /api/looper            { state, hasLoop, loopSeconds, position, level, maxSeconds }
 *   POST /api/looper/record     context-sensitive: record -> close -> overdub/play
 *   POST /api/looper/stop
 *   POST /api/looper/play
 *   POST /api/looper/clear
 *   PUT  /api/looper/level      { value: 0..100 }   playback level, %
 *
 * A single-loop phrase looper mixed in after the master stage. `state` is one of
 * empty | recording | playing | overdubbing | stopped.
 */
class LooperHandler final : public HttpHandler
{
public:
    explicit LooperHandler (milodikfx::dsp::LooperProcessor& looperToUse) : looper (looperToUse) {}

    /** Marked dirty so the playback level survives a restart. */
    std::function<void()> onChanged;

    Response handleGet (const std::string& path, const std::string&) const override
    {
        using namespace milodikfx::api;

        if (! pathSegmentsAfter (path, "/api/looper").empty())
            return jsonError (404, "Unknown looper endpoint");

        return jsonOk (stateVar());
    }

    Response handlePost (const std::string& path, const std::string&) override
    {
        using namespace milodikfx::api;
        using Action = milodikfx::dsp::LooperProcessor::Action;

        const auto segments = pathSegmentsAfter (path, "/api/looper");

        if (segments.size() != 1)
            return jsonError (404, "Expected /api/looper/<record|stop|play|clear>");

        const auto action = toLowerAscii (segments[0]);

        if (action == "record")     looper.requestAction (Action::record);
        else if (action == "stop")  looper.requestAction (Action::stop);
        else if (action == "play")  looper.requestAction (Action::play);
        else if (action == "clear") looper.requestAction (Action::clear);
        else                        return jsonError (404, "Unknown looper action");

        return jsonOk (stateVar());
    }

    Response handlePut (const std::string& path, const std::string& body) override
    {
        using namespace milodikfx::api;

        const auto segments = pathSegmentsAfter (path, "/api/looper");

        if (segments.size() != 1 || toLowerAscii (segments[0]) != "level")
            return jsonError (404, "Expected /api/looper/level");

        const auto parsed = parseBody (body);
        double value = 0.0;

        if (! readNumber (parsed, "value", value))
            return jsonError (400, "Body must contain a numeric 'value' (0-100)");

        looper.setLevelPercent ((float) value);

        if (onChanged)
            onChanged();

        return jsonOk (stateVar());
    }

private:
    static const char* stateName (milodikfx::dsp::LooperProcessor::State state)
    {
        using State = milodikfx::dsp::LooperProcessor::State;

        switch (state)
        {
            case State::recording:   return "recording";
            case State::playing:     return "playing";
            case State::overdubbing: return "overdubbing";
            case State::stopped:     return "stopped";
            case State::empty:
            default:                 return "empty";
        }
    }

    juce::var stateVar() const
    {
        auto* object = new juce::DynamicObject();
        object->setProperty ("state", stateName (looper.getState()));
        object->setProperty ("hasLoop", looper.hasLoop());
        object->setProperty ("loopSeconds", looper.getLoopSeconds());
        object->setProperty ("position", looper.getPositionFraction());
        object->setProperty ("level", looper.getLevelPercent());
        object->setProperty ("maxSeconds", (double) milodikfx::dsp::LooperProcessor::kMaxSeconds);

        return juce::var (object);
    }

    milodikfx::dsp::LooperProcessor& looper;
};
