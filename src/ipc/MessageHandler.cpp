#include "ipc/MessageHandler.h"
#include "ipc/Bridges.h"

namespace milodikfx::ipc
{

// DeviceMessageHandler
DeviceMessageHandler::DeviceMessageHandler(DeviceBridge* bridge)
    : deviceBridge(bridge)
{
}

juce::var DeviceMessageHandler::handleMessage(const juce::String& messageType, const juce::var& data)
{
    if (messageType == "device.list")
        return deviceBridge->listDevices();
    if (messageType == "device.select")
        return deviceBridge->selectDevice(data["id"].toString());
    if (messageType == "device.current")
        return deviceBridge->getCurrentDevice();
    if (messageType == "device.info")
        return deviceBridge->getDeviceInfo(data["id"].toString());

    return juce::var();
}

// PresetMessageHandler
PresetMessageHandler::PresetMessageHandler(PresetBridge* bridge)
    : presetBridge(bridge)
{
}

juce::var PresetMessageHandler::handleMessage(const juce::String& messageType, const juce::var& data)
{
    if (messageType == "preset.save")
        return presetBridge->savePreset(data["name"].toString(), data["data"]);
    if (messageType == "preset.load")
        return presetBridge->loadPreset(data["id"].toString());
    if (messageType == "preset.list")
        return presetBridge->listPresets();
    if (messageType == "preset.delete")
        return presetBridge->deletePreset(data["id"].toString());

    return juce::var();
}

// ParameterMessageHandler
ParameterMessageHandler::ParameterMessageHandler(ParameterBridge* bridge)
    : parameterBridge(bridge)
{
}

juce::var ParameterMessageHandler::handleMessage(const juce::String& messageType, const juce::var& data)
{
    if (messageType == "parameter.set")
        return parameterBridge->setParameter(
            data["effectId"].toString(),
            data["paramName"].toString(),
            (float)data["value"]);
    if (messageType == "parameter.get")
        return parameterBridge->getParameter(data["effectId"].toString(), data["paramName"].toString());
    if (messageType == "parameter.all")
        return parameterBridge->getAllParameters();

    return juce::var();
}

// EffectMessageHandler
EffectMessageHandler::EffectMessageHandler(EffectBridge* bridge)
    : effectBridge(bridge)
{
}

juce::var EffectMessageHandler::handleMessage(const juce::String& messageType, const juce::var& data)
{
    if (messageType == "effect.add")
        return effectBridge->addEffect(data["type"].toString(), (int)data["position"]);
    if (messageType == "effect.remove")
        return effectBridge->removeEffect(data["id"].toString());
    if (messageType == "effect.reorder")
    {
        juce::Array<juce::String> order;
        auto& arr = data;
        for (int i = 0; i < arr.size(); ++i)
            order.add(arr[i].toString());
        return effectBridge->reorderEffects(order);
    }
    if (messageType == "effect.list")
        return effectBridge->listEffects();

    return juce::var();
}

} // namespace milodikfx::ipc
