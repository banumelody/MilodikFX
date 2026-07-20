#include "ParametersHandler.h"

#include "../dsp/GainProcessor.h"
#include "../dsp/OverdriveProcessor.h"
#include "../dsp/EQProcessor.h"
#include "../dsp/CompressorProcessor.h"
#include "../dsp/ReverbProcessor.h"
#include "../dsp/ToneStackProcessor.h"

#include <algorithm>
#include <cctype>

static std::string buildMasterVolumeJson(float gainDb)
{
    return std::string(R"({"masterVolume":)") + std::to_string(gainDb) + "}";
}

static std::string jsonKeyValue(const std::string& k, const std::string& v)
{
    return "\"" + k + "\":" + v;
}

static std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

HttpHandler::Response ParametersHandler::handleGet(
    const std::string& path,
    const std::string& query) const
{
    try
    {
        auto& chain = engine_.getChain();

        // GET /api/parameters/master-volume
        if (path.find("/api/parameters/master-volume") != std::string::npos)
        {
            float masterVol = static_cast<float>(settings_.getDoubleValue("dsp.cleanBoost.gainDb", 0.0));
            if (auto* gp = chain.findProcessor<milodikfx::dsp::GainProcessor>())
                masterVol = gp->getGainDb();

            return { 200, "application/json", buildMasterVolumeJson(masterVol) };
        }

        // GET /api/parameters -> return summary of common parameters
        if (path == "/api/parameters" || path == "/api/parameters/")
        {
            std::string json = "{";
            // master volume
            float masterVol = static_cast<float>(settings_.getDoubleValue("dsp.cleanBoost.gainDb", 0.0));
            if (auto* gp = chain.findProcessor<milodikfx::dsp::GainProcessor>())
                masterVol = gp->getGainDb();
            json += jsonKeyValue("masterVolume", std::to_string(masterVol));

            // overdrive
            if (auto* od = chain.findProcessor<milodikfx::dsp::OverdriveProcessor>())
            {
                json += "," + jsonKeyValue("overdrive.drivePct", std::to_string(od->getDrivePercent()));
                json += "," + jsonKeyValue("overdrive.levelPct", std::to_string(od->getLevelPercent()));
                json += "," + jsonKeyValue("overdrive.enabled", od->isEnabled() ? "true" : "false");
            }

            // eq
            if (auto* eq = chain.findProcessor<milodikfx::dsp::EQProcessor>())
            {
                json += "," + jsonKeyValue("eq.bassDb", std::to_string(eq->getBassDb()));
                json += "," + jsonKeyValue("eq.midDb", std::to_string(eq->getMidDb()));
                json += "," + jsonKeyValue("eq.trebleDb", std::to_string(eq->getTrebleDb()));
                json += "," + jsonKeyValue("eq.enabled", eq->isEnabled() ? "true" : "false");
            }

            json += "}";
            return { 200, "application/json", json };
        }

        // GET single /api/parameters/{effect}/{param}
        if (path.find("/api/parameters/") == 0)
        {
            // e.g. /api/parameters/overdrive/drivePct
            size_t base = std::string("/api/parameters/").length();
            size_t slashPos = path.find('/', base);
            if (slashPos == std::string::npos)
                return { 400, "application/json", R"({"error":"Missing parameter name"})" };

            std::string effect = toLower(path.substr(base, slashPos - base));
            std::string param = path.substr(slashPos + 1);

            if (effect == "overdrive")
            {
                if (auto* od = chain.findProcessor<milodikfx::dsp::OverdriveProcessor>())
                {
                    if (param == "drivePct") return {200, "application/json", std::string("{\"value\":") + std::to_string(od->getDrivePercent()) + "}"};
                    if (param == "levelPct") return {200, "application/json", std::string("{\"value\":") + std::to_string(od->getLevelPercent()) + "}"};
                    if (param == "enabled") return {200, "application/json", std::string("{\"value\":") + (od->isEnabled() ? "true" : "false") + "}"};
                }
            }

            if (effect == "gain" || effect == "cleanboost")
            {
                if (auto* gp = chain.findProcessor<milodikfx::dsp::GainProcessor>())
                {
                    if (param == "gainDb") return {200, "application/json", std::string("{\"value\":") + std::to_string(gp->getGainDb()) + "}"};
                    if (param == "enabled") return {200, "application/json", std::string("{\"value\":") + (gp->isEnabled() ? "true" : "false") + "}"};
                }
            }

            if (effect == "eq")
            {
                if (auto* eq = chain.findProcessor<milodikfx::dsp::EQProcessor>())
                {
                    if (param == "bassDb") return {200, "application/json", std::string("{\"value\":") + std::to_string(eq->getBassDb()) + "}"};
                    if (param == "midDb") return {200, "application/json", std::string("{\"value\":") + std::to_string(eq->getMidDb()) + "}"};
                    if (param == "trebleDb") return {200, "application/json", std::string("{\"value\":") + std::to_string(eq->getTrebleDb()) + "}"};
                    if (param == "enabled") return {200, "application/json", std::string("{\"value\":") + (eq->isEnabled() ? "true" : "false") + "}"};
                }
            }

            return { 404, "application/json", R"({"error":"Parameter not found"})" };
        }

        return { 404, "application/json", R"({"error":"Parameter not found"})" };
    }
    catch (const std::exception& e)
    {
        return { 500, "application/json", std::string(R"({"error":"Exception: )") + e.what() + R"("})" };
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
            return { 400, "application/json", R"({"error":"Missing 'value' field"})" };
        }

        size_t colonPos = body.find(":", valuePos);
        size_t numStart = body.find_first_of("-0123456789", colonPos);
        size_t numEnd = body.find_first_not_of("-0123456789.", numStart);

        if (numStart == std::string::npos || numEnd == std::string::npos)
        {
            return { 400, "application/json", R"({"error":"Invalid value format"})" };
        }

        std::string valueStr = body.substr(numStart, numEnd - numStart);
        float value = std::stof(valueStr);

        auto& chain = engine_.getChain();

        // Handle /api/parameters/master-volume
        if (path.find("/api/parameters/master-volume") != std::string::npos)
        {
            // Apply to GainProcessor if present, else persist to settings
            if (auto* gp = chain.findProcessor<milodikfx::dsp::GainProcessor>())
            {
                gp->setGainDb(value);
            }
            settings_.setValue("dsp.cleanBoost.gainDb", static_cast<double>(value));
            settings_.saveIfNeeded();

            return { 200, "application/json", buildMasterVolumeJson(value) };
        }

        // Handle /api/parameters/{effect}/{param}
        if (path.find("/api/parameters/") == 0)
        {
            size_t base = std::string("/api/parameters/").length();
            size_t slashPos = path.find('/', base);
            if (slashPos == std::string::npos)
                return { 400, "application/json", R"({"error":"Missing parameter name"})" };

            std::string effect = toLower(path.substr(base, slashPos - base));
            std::string param = path.substr(slashPos + 1);

            if (effect == "overdrive")
            {
                if (auto* od = chain.findProcessor<milodikfx::dsp::OverdriveProcessor>())
                {
                    if (param == "drivePct") od->setDrivePercent(value);
                    else if (param == "levelPct") od->setLevelPercent(value);
                    else return { 400, "application/json", R"({"error":"Unknown param"})" };

                    return { 200, "application/json", std::string("{\"effect\": \"") + effect + "\", \"param\": \"" + param + "\", \"value\": " + valueStr + " }" };
                }
                else
                {
                    return { 404, "application/json", R"({"error":"Effect not present"})" };
                }
            }

            if (effect == "gain" || effect == "cleanboost")
            {
                if (auto* gp = chain.findProcessor<milodikfx::dsp::GainProcessor>())
                {
                    if (param == "gainDb") gp->setGainDb(value);
                    else return { 400, "application/json", R"({"error":"Unknown param"})" };

                    return { 200, "application/json", std::string("{\"effect\": \"") + effect + "\", \"param\": \"" + param + "\", \"value\": " + valueStr + " }" };
                }
                else
                {
                    return { 404, "application/json", R"({"error":"Effect not present"})" };
                }
            }

            if (effect == "eq")
            {
                if (auto* eq = chain.findProcessor<milodikfx::dsp::EQProcessor>())
                {
                    if (param == "bassDb") eq->setBassDb(value);
                    else if (param == "midDb") eq->setMidDb(value);
                    else if (param == "trebleDb") eq->setTrebleDb(value);
                    else return { 400, "application/json", R"({"error":"Unknown param"})" };

                    return { 200, "application/json", std::string("{\"effect\": \"") + effect + "\", \"param\": \"" + param + "\", \"value\": " + valueStr + " }" };
                }
                else
                {
                    return { 404, "application/json", R"({"error":"Effect not present"})" };
                }
            }

            return { 404, "application/json", R"({"error":"Effect not found"})" };
        }

        return { 404, "application/json", R"({"error":"Parameter endpoint not found"})" };
    }
    catch (const std::exception& e)
    {
        return { 500, "application/json", std::string(R"({"error":"Exception: )") + e.what() + R"("})" };
    }
}
