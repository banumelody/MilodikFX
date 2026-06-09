#include "ParametersHandler.h"

/**
 * Simple JSON builder for master volume parameter
 */
static std::string buildMasterVolumeJson(float gainDb)
{
    return std::string(R"({"masterVolume":)") + std::to_string(gainDb) + "}";
}

HttpHandler::Response ParametersHandler::handleGet(
    const std::string& path,
    const std::string& query) const
{
    try
    {
        // For now, just support /api/parameters/master-volume
        if (path.find("/api/parameters/master-volume") != std::string::npos)
        {
            // TODO: Get actual value from GainProcessor
            // For now, return mock value from settings
            float masterVol = static_cast<float>(settings_.getDoubleValue("dsp.cleanBoost.gainDb", 0.0));
            
            return {
                200,
                "application/json",
                buildMasterVolumeJson(masterVol)
            };
        }
        
        return {
            404,
            "application/json",
            R"({"error":"Parameter not found"})"
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

HttpHandler::Response ParametersHandler::handlePut(
    const std::string& path,
    const std::string& body)
{
    try
    {
        // Extract value from JSON body: {"value": 3.5}
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
        
        if (numStart == std::string::npos || numEnd == std::string::npos)
        {
            return {
                400,
                "application/json",
                R"({"error":"Invalid value format"})"
            };
        }
        
        std::string valueStr = body.substr(numStart, numEnd - numStart);
        float value = std::stof(valueStr);
        
        // Handle /api/parameters/master-volume
        if (path.find("/api/parameters/master-volume") != std::string::npos)
        {
            // TODO: Apply to actual GainProcessor
            // For now, save to settings
            settings_.setValue("dsp.cleanBoost.gainDb", static_cast<double>(value));
            settings_.saveIfNeeded();
            
            return {
                200,
                "application/json",
                buildMasterVolumeJson(value)
            };
        }
        
        return {
            404,
            "application/json",
            R"({"error":"Parameter endpoint not found"})"
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
