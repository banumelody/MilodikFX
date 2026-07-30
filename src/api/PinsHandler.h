#pragma once

#include <functional>

#include "api/ApiJson.h"
#include "api/HttpHandler.h"
#include "api/ParameterRegistry.h"
#include "preset/PinnedControls.h"

/**
 * /api/pins
 *
 *   GET  /api/pins             { max, pins: [{ effect, parameter, effectLabel, label, unit, min, max, step, value }] }
 *   PUT  /api/pins             { pins: [{ effect, parameter }] }   replaces the list
 *   POST /api/pins/toggle      { effect, parameter }               adds or removes one
 *
 * The knobs a preset wants on the stage screen. The response carries enough
 * about each one for the Perform view to draw it without going back to
 * /api/effects and picking through the whole rack.
 */
class PinsHandler final : public HttpHandler
{
public:
    PinsHandler (milodikfx::preset::PinnedControls& pinsToUse,
                 const milodikfx::api::ParameterRegistry& registryToUse)
        : pins (pinsToUse), registry (registryToUse)
    {
    }

    /** Marked dirty so the pins survive a restart and reach the preset file. */
    std::function<void()> onChanged;

    Response handleGet (const std::string& path, const std::string&) const override
    {
        using namespace milodikfx::api;

        if (! pathSegmentsAfter (path, "/api/pins").empty())
            return jsonError (404, "Unknown pins endpoint");

        return jsonOk (stateVar());
    }

    Response handlePut (const std::string& path, const std::string& body) override
    {
        using namespace milodikfx::api;

        if (! pathSegmentsAfter (path, "/api/pins").empty())
            return jsonError (404, "Unknown pins endpoint");

        const auto parsed = parseBody (body);
        const auto list = parsed["pins"];

        if (! list.isArray())
            return jsonError (400, "Body must contain a 'pins' array");

        // Validated against the registry, so a pin can never name a control that
        // does not exist -- the stage screen would have nothing to draw.
        pins.fromVar (list, &registry);
        notify();

        return jsonOk (stateVar());
    }

    Response handlePost (const std::string& path, const std::string& body) override
    {
        using namespace milodikfx::api;

        const auto segments = pathSegmentsAfter (path, "/api/pins");

        if (segments.size() != 1 || toLowerAscii (segments[0]) != "toggle")
            return jsonError (404, "Expected /api/pins/toggle");

        const auto parsed = parseBody (body);
        const auto effectId = parsed["effect"].toString().toStdString();
        const auto parameterId = parsed["parameter"].toString().toStdString();

        if (effectId.empty() || parameterId.empty())
            return jsonError (400, "Body must contain 'effect' and 'parameter'");

        if (registry.findParameter (effectId, parameterId) == nullptr)
            return jsonError (404, "No such parameter");

        const auto* descriptor = registry.findParameter (effectId, parameterId);

        // A file chooser has no knob to put on the stage screen.
        if (descriptor->isText)
            return jsonError (400, "A text parameter cannot be pinned");

        if (! pins.isPinned (effectId, parameterId)
            && pins.getNumPins() >= milodikfx::preset::PinnedControls::kMaxPins)
            return jsonError (409, "All " + juce::String (milodikfx::preset::PinnedControls::kMaxPins)
                                       + " pins are in use");

        pins.toggle (effectId, parameterId);
        notify();

        return jsonOk (stateVar());
    }

private:
    void notify() const
    {
        if (onChanged)
            onChanged();
    }

    juce::var stateVar() const
    {
        juce::Array<juce::var> array;

        for (int i = 0; i < pins.getNumPins(); ++i)
        {
            const auto& pin = pins.getPin (i);
            const auto* effect = registry.findEffect (pin.effectId);
            const auto* parameter = registry.findParameter (pin.effectId, pin.parameterId);

            if (effect == nullptr || parameter == nullptr)
                continue;

            auto* entry = new juce::DynamicObject();
            entry->setProperty ("effect", juce::String (pin.effectId));
            entry->setProperty ("parameter", juce::String (pin.parameterId));
            entry->setProperty ("effectLabel", juce::String (effect->label));
            entry->setProperty ("label", juce::String (parameter->label));
            entry->setProperty ("unit", juce::String (parameter->unit));
            entry->setProperty ("min", parameter->minValue);
            entry->setProperty ("max", parameter->maxValue);
            entry->setProperty ("step", parameter->step);
            entry->setProperty ("isBoolean", parameter->isBoolean);
            entry->setProperty ("value", parameter->get ? parameter->get() : 0.0f);

            array.add (juce::var (entry));
        }

        auto* root = new juce::DynamicObject();
        root->setProperty ("max", milodikfx::preset::PinnedControls::kMaxPins);
        root->setProperty ("pins", array);

        return juce::var (root);
    }

    milodikfx::preset::PinnedControls& pins;
    const milodikfx::api::ParameterRegistry& registry;
};
