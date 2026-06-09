#include "DevicesHandler.h"

/**
 * Simple JSON builder (without external library dependency)
 */
static std::string buildDevicesJson(
    const juce::StringArray& inputDevices,
    const juce::StringArray& outputDevices,
    const std::string& selectedInput,
    const std::string& selectedOutput,
    double sampleRate,
    int bufferSize)
{
    std::string json = "{\n";
    
    // Input devices array
    json += "  \"input\": [";
    for (int i = 0; i < inputDevices.size(); ++i) {
        json += "\"" + inputDevices[i].toStdString() + "\"";
        if (i < inputDevices.size() - 1) json += ", ";
    }
    json += "],\n";
    
    // Output devices array
    json += "  \"output\": [";
    for (int i = 0; i < outputDevices.size(); ++i) {
        json += "\"" + outputDevices[i].toStdString() + "\"";
        if (i < outputDevices.size() - 1) json += ", ";
    }
    json += "],\n";
    
    // Selected devices
    json += "  \"selectedInput\": \"" + selectedInput + "\",\n";
    json += "  \"selectedOutput\": \"" + selectedOutput + "\",\n";
    json += "  \"sampleRate\": " + std::to_string(static_cast<int>(sampleRate)) + ",\n";
    json += "  \"bufferSize\": " + std::to_string(bufferSize) + "\n";
    json += "}";
    
    return json;
}

HttpHandler::Response DevicesHandler::handleGet(
    const std::string& path,
    const std::string& query) const
{
    try
    {
        // Get current setup
        auto setup = deviceManager_.getAudioDeviceSetup();
        
        juce::StringArray inputDevices, outputDevices;
        
        // Enumerate devices for the current device type
        if (auto* type = deviceManager_.getCurrentDeviceTypeObject())
        {
            type->scanForDevices();
            inputDevices = type->getDeviceNames(true);   // true = input
            outputDevices = type->getDeviceNames(false); // false = output
        }
        else
        {
            return {
                500,
                "application/json",
                R"({"error":"No audio device type available"})"
            };
        }
        
        std::string selectedInput = setup.inputDeviceName.toStdString();
        std::string selectedOutput = setup.outputDeviceName.toStdString();
        if (selectedInput.empty()) selectedInput = "Default";
        if (selectedOutput.empty()) selectedOutput = "Default";
        
        std::string json = buildDevicesJson(
            inputDevices,
            outputDevices,
            selectedInput,
            selectedOutput,
            setup.sampleRate,
            setup.bufferSize
        );
        
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

HttpHandler::Response DevicesHandler::handlePost(
    const std::string& path,
    const std::string& body)
{
    try
    {
        // Parse simple JSON body: {"input": "device1", "output": "device2"}
        // For now, use basic string parsing (TODO: use JSON library)
        
        std::string inputDevice, outputDevice;
        
        // Extract "input" value
        size_t inputPos = body.find("\"input\"");
        if (inputPos != std::string::npos)
        {
            size_t colonPos = body.find(":", inputPos);
            size_t quoteStart = body.find("\"", colonPos);
            size_t quoteEnd = body.find("\"", quoteStart + 1);
            if (quoteStart != std::string::npos && quoteEnd != std::string::npos)
            {
                inputDevice = body.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
            }
        }
        
        // Extract "output" value
        size_t outputPos = body.find("\"output\"");
        if (outputPos != std::string::npos)
        {
            size_t colonPos = body.find(":", outputPos);
            size_t quoteStart = body.find("\"", colonPos);
            size_t quoteEnd = body.find("\"", quoteStart + 1);
            if (quoteStart != std::string::npos && quoteEnd != std::string::npos)
            {
                outputDevice = body.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
            }
        }
        
        if (inputDevice.empty() && outputDevice.empty())
        {
            return {
                400,
                "application/json",
                R"({"error":"No input or output device specified"})"
            };
        }
        
        // Try to set the new device
        juce::String error;
        auto setup = deviceManager_.getAudioDeviceSetup();
        
        if (!inputDevice.empty())
            setup.inputDeviceName = juce::String(inputDevice);
        if (!outputDevice.empty())
            setup.outputDeviceName = juce::String(outputDevice);
        
        error = deviceManager_.setAudioDeviceSetup(setup, true);
        
        if (error.isNotEmpty())
        {
            return {
                400,
                "application/json",
                std::string(R"({"error":")" + error.toStdString() + R"("})")
            };
        }
        
        return {
            200,
            "application/json",
            R"({"success":true,"message":"Device changed"})"
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
