#include "LevelsHandler.h"

HttpHandler::Response LevelsHandler::handleGet(
    const std::string& path,
    const std::string& query) const
{
    try
    {
        float inLevel = inputLevel_.load(std::memory_order_relaxed);
        float outLevel = outputLevel_.load(std::memory_order_relaxed);
        
        // Build JSON response
        std::string json = "{\n";
        json += "  \"inputLevel\": " + std::to_string(inLevel) + ",\n";
        json += "  \"outputLevel\": " + std::to_string(outLevel) + "\n";
        json += "}";
        
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
