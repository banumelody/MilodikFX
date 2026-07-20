#include "EffectsHandler.h"

#include "../dsp/OverdriveProcessor.h"
#include "../dsp/GainProcessor.h"
#include "../dsp/EQProcessor.h"
#include "../dsp/CompressorProcessor.h"
#include "../dsp/ReverbProcessor.h"
#include "../dsp/ToneStackProcessor.h"

/**
 * Build JSON response for effect state
 */
static std::string buildEffectStateJson(
    const std::string& effect,
    bool enabled,
    const std::string& params = "")
{
    std::string json = "{\n";
    json += "  \"effect\": \"" + effect + "\",\n";
    json += "  \"enabled\": " + std::string(enabled ? "true" : "false") + "\n";
    if (!params.empty())
    {
        json += ",\n  \"parameters\": " + params;
    }
    json += "\n}";
    return json;
}

static std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

HttpHandler::Response EffectsHandler::handleGet(
    const std::string& path,
    const std::string& query) const
{
    // GET /api/effects/{effect} - return effect state (best-effort)
    try
    {
        auto& chain = chain_;
        size_t base = std::string("/api/effects/").length();
        if (path.size() <= base) return { 400, "application/json", R"({"error":"Invalid effect path"})" };
        size_t slash = path.find('/', base);
        std::string effect = (slash == std::string::npos) ? path.substr(base) : path.substr(base, slash - base);
        std::string eff = toLower(effect);

        if (eff == "overdrive")
        {
            if (auto* od = chain_.findProcessor<milodikfx::dsp::OverdriveProcessor>())
            {
                std::string params = "{\"drivePct\":" + std::to_string(od->getDrivePercent()) + ",\"levelPct\":" + std::to_string(od->getLevelPercent()) + "}";
                return {200, "application/json", buildEffectStateJson(effect, od->isEnabled(), params)};
            }
        }

        if (eff == "gain")
        {
            if (auto* gp = chain_.findProcessor<milodikfx::dsp::GainProcessor>())
            {
                std::string params = "{\"gainDb\":" + std::to_string(gp->getGainDb()) + "}";
                return {200, "application/json", buildEffectStateJson(effect, gp->isEnabled(), params)};
            }
        }

        return {200, "application/json", R"({"message":"Effect GET not yet implemented for this type"})" };
    }
    catch (const std::exception& e)
    {
        return {500, "application/json", std::string(R"({"error":"Exception: )") + e.what() + R"("})" };
    }
}

HttpHandler::Response EffectsHandler::handlePost(
    const std::string& path,
    const std::string& body)
{
    try
    {
        // POST /api/effects/{effect}/enabled - Toggle effect
        // Body: {"enabled": true/false}

        size_t effectStart = path.find("/api/effects/");
        if (effectStart == std::string::npos)
            return {400, "application/json", R"({"error":"Invalid path"})"};

        size_t effectNameStart = effectStart + 13; // length of "/api/effects/"
        size_t effectNameEnd = path.find("/", effectNameStart);
        if (effectNameEnd == std::string::npos)
            effectNameEnd = path.length();

        std::string effectName = path.substr(effectNameStart, effectNameEnd - effectNameStart);
        std::string eff = toLower(effectName);

        bool enabled = body.find("\"enabled\":true") != std::string::npos;

        // Apply to chain
        if (eff == "overdrive")
        {
            if (auto* od = chain_.findProcessor<milodikfx::dsp::OverdriveProcessor>())
            {
                od->setEnabled(enabled);
                return {200, "application/json", buildEffectStateJson(effectName, enabled)};
            }
            return {404, "application/json", R"({"error":"Overdrive not present"})"};
        }

        if (eff == "gain" || eff == "cleanboost")
        {
            if (auto* gp = chain_.findProcessor<milodikfx::dsp::GainProcessor>())
            {
                gp->setEnabled(enabled);
                return {200, "application/json", buildEffectStateJson(effectName, enabled)};
            }
            return {404, "application/json", R"({"error":"Gain not present"})"};
        }

        if (eff == "eq")
        {
            if (auto* eq = chain_.findProcessor<milodikfx::dsp::EQProcessor>())
            {
                eq->setEnabled(enabled);
                return {200, "application/json", buildEffectStateJson(effectName, enabled)};
            }
            return {404, "application/json", R"({"error":"EQ not present"})"};
        }

        if (eff == "compressor" || eff == "comp")
        {
            if (auto* cp = chain_.findProcessor<milodikfx::dsp::CompressorProcessor>())
            {
                cp->setEnabled(enabled);
                return {200, "application/json", buildEffectStateJson(effectName, enabled)};
            }
            return {404, "application/json", R"({"error":"Compressor not present"})"};
        }

        if (eff == "reverb")
        {
            if (auto* rp = chain_.findProcessor<milodikfx::dsp::ReverbProcessor>())
            {
                rp->setEnabled(enabled);
                return {200, "application/json", buildEffectStateJson(effectName, enabled)};
            }
            return {404, "application/json", R"({"error":"Reverb not present"})"};
        }

        return {400, "application/json", R"({"error":"Unknown effect"})"};
    }
    catch (const std::exception& e)
    {
        return {500, "application/json", std::string(R"({"error":"Exception: )") + e.what() + R"("})" };
    }
}

HttpHandler::Response EffectsHandler::handlePut(
    const std::string& path,
    const std::string& body)
{
    try
    {
        // PUT /api/effects/{effect}/{param} - Set effect parameter
        // Body: {"value": 50.5}
        
        // Extract effect and param from path
        // Example: /api/effects/overdrive/drivePct
        size_t effectStart = path.find("/api/effects/");
        if (effectStart == std::string::npos)
        {
            return {
                400,
                "application/json",
                R"({"error":"Invalid path"})"
            };
        }
        
        size_t effectNameStart = effectStart + 13; // "/api/effects/"
        size_t slashPos = path.find("/", effectNameStart);
        if (slashPos == std::string::npos)
        {
            return {
                400,
                "application/json",
                R"({"error":"Missing parameter name"})"
            };
        }
        
        std::string effectName = path.substr(effectNameStart, slashPos - effectNameStart);
        std::string paramName = path.substr(slashPos + 1);
        
        // Extract value from body
        size_t valuePos = body.find("\"value\"");
        if (valuePos == std::string::npos)
        {
            return {
                400,
                "application/json",
                R"({"error":"Missing 'value' field"})"
            };
        }
        
        size_t colonPos = body.find(":", valuePos);
        size_t numStart = body.find_first_of("-0123456789", colonPos);
        size_t numEnd = body.find_first_not_of("-0123456789.", numStart);
        
        if (numStart == std::string::npos)
        {
            return {
                400,
                "application/json",
                R"({"error":"Invalid value format"})"
            };
        }
        
        std::string valueStr = body.substr(numStart, numEnd - numStart);
        float value = std::stof(valueStr);
        
        // TODO: Apply parameter to actual effect processor
        // For now, just return success
        
        return {
            200,
            "application/json",
            "{\n  \"effect\": \"" + effectName + "\",\n  \"param\": \"" + paramName + 
            "\",\n  \"value\": " + valueStr + "\n}"
        };
    }
    catch (const std::exception& e)
    {
        return {
            500,
            "application/json",
            std::string(R"({"error":"Exception: )") + e.what() + R"("})"
        };
    }
}
