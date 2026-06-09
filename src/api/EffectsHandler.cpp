#include "EffectsHandler.h"

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

HttpHandler::Response EffectsHandler::handleGet(
    const std::string& path,
    const std::string& query) const
{
    // GET /api/effects/{effect} - return effect state
    // For now, return stub response
    return {
        200,
        "application/json",
        R"({"message":"Effect GET not yet implemented"})"
    };
}

HttpHandler::Response EffectsHandler::handlePost(
    const std::string& path,
    const std::string& body)
{
    try
    {
        // POST /api/effects/{effect}/enabled - Toggle effect
        // Body: {"enabled": true/false}
        
        // Extract effect name from path: /api/effects/overdrive/enabled
        size_t effectStart = path.find("/api/effects/");
        if (effectStart == std::string::npos)
        {
            return {
                400,
                "application/json",
                R"({"error":"Invalid path"})"
            };
        }
        
        size_t effectNameStart = effectStart + 13; // length of "/api/effects/"
        size_t effectNameEnd = path.find("/", effectNameStart);
        if (effectNameEnd == std::string::npos)
            effectNameEnd = path.length();
        
        std::string effectName = path.substr(effectNameStart, effectNameEnd - effectNameStart);
        
        // Extract enabled value from body
        bool enabled = body.find("\"enabled\":true") != std::string::npos;
        
        // TODO: Actually toggle effect in chain
        // For now, just return success
        
        return {
            200,
            "application/json",
            buildEffectStateJson(effectName, enabled)
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
