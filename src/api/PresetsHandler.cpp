#include "PresetsHandler.h"

HttpHandler::Response PresetsHandler::handleGet(
    const std::string& path,
    const std::string& query) const
{
    try
    {
        // GET /api/presets - List all presets
        std::string json = "{\n  \"presets\": [";
        
        auto files = presetsDir_.findChildFiles(juce::File::findFiles, false, "*.json");
        for (int i = 0; i < files.size(); ++i)
        {
            std::string presetName = files[i].getFileNameWithoutExtension().toStdString();
            json += "\"" + presetName + "\"";
            if (i < files.size() - 1) json += ", ";
        }
        
        json += "]\n}";
        
        return { 200, "application/json", json };
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

HttpHandler::Response PresetsHandler::handlePost(
    const std::string& path,
    const std::string& body)
{
    try
    {
        // Extract preset name from body: {"name": "MyPreset"}
        size_t namePos = body.find("\"name\"");
        if (namePos == std::string::npos)
        {
            return {
                400,
                "application/json",
                R"({"error":"Missing 'name' field"})"
            };
        }
        
        size_t colonPos = body.find(":", namePos);
        size_t quoteStart = body.find("\"", colonPos);
        size_t quoteEnd = body.find("\"", quoteStart + 1);
        
        if (quoteStart == std::string::npos || quoteEnd == std::string::npos)
        {
            return {
                400,
                "application/json",
                R"({"error":"Invalid name format"})"
            };
        }
        
        std::string presetName = body.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        
        // Determine action based on path
        if (path.find("/presets/save") != std::string::npos)
        {
            // POST /api/presets/save - Save preset
            // TODO: Serialize current DSP state to JSON
            // For now, just create an empty preset file
            juce::File presetFile = presetsDir_.getChildFile(presetName + ".json");
            presetFile.replaceWithText(R"({"preset":")" + presetName + R"("})");
            
            return {
                200,
                "application/json",
                "{\n  \"success\": true,\n  \"message\": \"Preset '" + presetName + "' saved\"\n}"
            };
        }
        else if (path.find("/presets/load") != std::string::npos)
        {
            // POST /api/presets/load - Load preset
            juce::File presetFile = presetsDir_.getChildFile(presetName + ".json");
            
            if (!presetFile.existsAsFile())
            {
                return {
                    404,
                    "application/json",
                    "{\n  \"success\": false,\n  \"message\": \"Preset '" + presetName + "' not found\"\n}"
                };
            }
            
            // TODO: Deserialize and apply to DSP chain
            
            return {
                200,
                "application/json",
                "{\n  \"success\": true,\n  \"message\": \"Preset '" + presetName + "' loaded\"\n}"
            };
        }
        
        return {
            400,
            "application/json",
            R"({"error":"Unknown preset action"})"
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
