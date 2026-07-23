#pragma once

#include <functional>
#include <string>

#include "api/ApiJson.h"
#include "api/HttpHandler.h"
#include "api/ParameterRegistry.h"
#include "dsp/ModulationEngine.h"

/**
 * /api/modifiers
 *
 *   GET    /api/modifiers          every slot, including source/low/high/rateHz,
 *                                   expressionCc, syncDivision, baseOffset, base
 *   PUT    /api/modifiers/<slot>   { effect, parameter, source, low, high, rateHz,
 *                                    expressionCc?, syncDivision? }
 *   DELETE /api/modifiers/<slot>
 *
 * A modifier sweeps one numeric parameter with an LFO, the envelope of the input,
 * or an expression pedal. Source is a string: lfoSine | lfoTriangle | lfoSquare |
 * envelope | expression. `syncDivision` locks an LFO to the tempo (0 = free);
 * `expressionCc` is the CC an expression source follows.
 */
class ModulationHandler final : public HttpHandler
{
public:
    ModulationHandler (const milodikfx::api::ParameterRegistry& registry,
                       milodikfx::dsp::ModulationEngine& engine)
        : registry_ (registry), engine_ (engine)
    {
    }

    /** Marked dirty so a modifier survives a restart, like a MIDI mapping. */
    std::function<void()> onChanged;

    Response handleGet (const std::string& path, const std::string&) const override
    {
        using milodikfx::api::jsonError;
        using milodikfx::api::jsonOk;

        const auto segments = milodikfx::api::pathSegmentsAfter (path, "/api/modifiers");

        if (! segments.empty())
            return jsonError (404, "Unknown modifiers endpoint");

        return jsonOk (stateVar());
    }

    Response handlePut (const std::string& path, const std::string& body) override
    {
        using namespace milodikfx::api;

        const auto slot = parseSlot (path);

        if (slot < 0)
            return jsonError (404, "Expected /api/modifiers/<0-3>");

        const auto parsed = parseBody (body);

        if (! parsed.isObject())
            return jsonError (400, "Expected a modifier object");

        const auto effectId = parsed["effect"].toString().trim().toStdString();
        const auto parameterId = parsed["parameter"].toString().trim().toStdString();

        const auto* target = registry_.findParameter (effectId, parameterId);

        if (target == nullptr)
            return jsonError (404, "Unknown effect or parameter");

        double low = 0.0, high = 0.0, rate = 1.0, expressionCc = -1.0, syncDivision = 0.0;
        readNumber (parsed, "low", low);
        readNumber (parsed, "high", high);

        if (! readNumber (parsed, "rateHz", rate))
            rate = 1.0;

        readNumber (parsed, "expressionCc", expressionCc);
        readNumber (parsed, "syncDivision", syncDivision);

        milodikfx::dsp::ModulationEngine::Config config;
        config.source = parseSource (parsed["source"].toString());
        config.low = (float) low;
        config.high = (float) high;
        config.rateHz = (float) rate;
        config.expressionCc = (int) expressionCc;
        config.syncDivision = (int) syncDivision;
        // baseOffset defaults to 0: a fresh modifier sweeps exactly low..high, and
        // the knob shifts it from there.

        if (! engine_.setModifier (slot, target,
                                   // Store under the registry's own casing so the
                                   // reported binding matches the effect list.
                                   registry_.findEffect (effectId)->id, target->id, config))
            return jsonError (400, "That parameter cannot be modulated (a switch or a file is not sweepable)");

        if (onChanged)
            onChanged();

        return jsonOk (stateVar());
    }

    Response handleDelete (const std::string& path) override
    {
        using milodikfx::api::jsonError;
        using milodikfx::api::jsonOk;

        const auto slot = parseSlot (path);

        if (slot < 0)
            return jsonError (404, "Expected /api/modifiers/<0-3>");

        engine_.clearModifier (slot);

        if (onChanged)
            onChanged();

        return jsonOk (stateVar());
    }

private:
    static juce::String sourceName (int source)
    {
        switch ((milodikfx::dsp::ModulationEngine::Source) source)
        {
            case milodikfx::dsp::ModulationEngine::Source::lfoTriangle: return "lfoTriangle";
            case milodikfx::dsp::ModulationEngine::Source::lfoSquare:   return "lfoSquare";
            case milodikfx::dsp::ModulationEngine::Source::envelope:    return "envelope";
            case milodikfx::dsp::ModulationEngine::Source::expression:  return "expression";
            default:                                                    return "lfoSine";
        }
    }

    static milodikfx::dsp::ModulationEngine::Source parseSource (const juce::String& value)
    {
        using Source = milodikfx::dsp::ModulationEngine::Source;

        const auto v = value.trim().toLowerCase();

        if (v == "lfotriangle") return Source::lfoTriangle;
        if (v == "lfosquare")   return Source::lfoSquare;
        if (v == "envelope")    return Source::envelope;
        if (v == "expression")  return Source::expression;

        return Source::lfoSine;
    }

    static int parseSlot (const std::string& path)
    {
        const auto segments = milodikfx::api::pathSegmentsAfter (path, "/api/modifiers");

        if (segments.size() != 1)
            return -1;

        const juce::String text (segments[0]);

        if (text.isEmpty() || ! text.containsOnly ("0123456789"))
            return -1;

        const auto slot = text.getIntValue();

        return milodikfx::dsp::ModulationEngine::isValidSlot (slot) ? slot : -1;
    }

    juce::var stateVar() const
    {
        juce::Array<juce::var> modifiers;

        for (int slot = 0; slot < milodikfx::dsp::ModulationEngine::kMaxModifiers; ++slot)
        {
            const auto info = engine_.getModifier (slot);

            auto* object = new juce::DynamicObject();
            object->setProperty ("slot", slot);
            object->setProperty ("active", info.active);
            object->setProperty ("effect", juce::String (info.effectId));
            object->setProperty ("parameter", juce::String (info.parameterId));
            object->setProperty ("source", sourceName (info.source));
            object->setProperty ("low", info.low);
            object->setProperty ("high", info.high);
            object->setProperty ("rateHz", info.rateHz);
            object->setProperty ("expressionCc", info.expressionCc);
            object->setProperty ("syncDivision", info.syncDivision);
            object->setProperty ("baseOffset", info.baseOffset);
            object->setProperty ("base", info.base);

            modifiers.add (juce::var (object));
        }

        auto* root = new juce::DynamicObject();
        root->setProperty ("modifiers", modifiers);

        return juce::var (root);
    }

    const milodikfx::api::ParameterRegistry& registry_;
    milodikfx::dsp::ModulationEngine& engine_;
};
