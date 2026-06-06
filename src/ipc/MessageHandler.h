#pragma once

#include <JuceHeader.h>
#include <functional>
#include <map>
#include <memory>

namespace milodikfx::ipc
{

class MessageHandler
{
public:
    using Handler = std::function<juce::var(const juce::var&)>;

    virtual ~MessageHandler() = default;
    virtual juce::var handleMessage(const juce::String& messageType, const juce::var& data) = 0;
};

class DeviceMessageHandler : public MessageHandler
{
public:
    explicit DeviceMessageHandler(class DeviceBridge* bridge);
    juce::var handleMessage(const juce::String& messageType, const juce::var& data) override;

private:
    DeviceBridge* deviceBridge;
};

class PresetMessageHandler : public MessageHandler
{
public:
    explicit PresetMessageHandler(class PresetBridge* bridge);
    juce::var handleMessage(const juce::String& messageType, const juce::var& data) override;

private:
    PresetBridge* presetBridge;
};

class ParameterMessageHandler : public MessageHandler
{
public:
    explicit ParameterMessageHandler(class ParameterBridge* bridge);
    juce::var handleMessage(const juce::String& messageType, const juce::var& data) override;

private:
    ParameterBridge* parameterBridge;
};

class EffectMessageHandler : public MessageHandler
{
public:
    explicit EffectMessageHandler(class EffectBridge* bridge);
    juce::var handleMessage(const juce::String& messageType, const juce::var& data) override;

private:
    EffectBridge* effectBridge;
};

} // namespace milodikfx::ipc
