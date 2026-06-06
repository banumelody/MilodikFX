#include "ipc/Bridges.h"
#include "preset/PresetManager.h"

namespace milodikfx::ipc
{

// DeviceBridge
DeviceBridge::DeviceBridge(juce::AudioDeviceManager& manager)
    : deviceManager(manager)
{
}

juce::var DeviceBridge::listDevices()
{
    auto devices = new juce::DynamicObject();
    juce::var inputDevices;
    juce::var outputDevices;

    if (auto* type = deviceManager.getAvailableDeviceTypes()[0])
    {
        auto inputNames = type->getDeviceNames(true);
        auto outputNames = type->getDeviceNames(false);

        for (int i = 0; i < inputNames.size(); ++i)
        {
            auto device = new juce::DynamicObject();
            device->setProperty("id", inputNames[i]);
            device->setProperty("name", inputNames[i]);
            device->setProperty("isInput", true);
            inputDevices.append(device);
        }

        for (int i = 0; i < outputNames.size(); ++i)
        {
            auto device = new juce::DynamicObject();
            device->setProperty("id", outputNames[i]);
            device->setProperty("name", outputNames[i]);
            device->setProperty("isInput", false);
            outputDevices.append(device);
        }
    }

    devices->setProperty("inputs", inputDevices);
    devices->setProperty("outputs", outputDevices);
    return juce::var(devices);
}

juce::var DeviceBridge::selectDevice(const juce::String& deviceId)
{
    auto result = new juce::DynamicObject();
    result->setProperty("success", true);
    result->setProperty("deviceId", deviceId);
    return juce::var(result);
}

juce::var DeviceBridge::getCurrentDevice()
{
    auto device = new juce::DynamicObject();
    if (auto* current = deviceManager.getCurrentAudioDevice())
    {
        device->setProperty("name", current->getName());
        device->setProperty("sampleRate", (int)current->getCurrentSampleRate());
        device->setProperty("bufferSize", current->getCurrentBufferSizeSamples());
        device->setProperty("numInputChannels", current->getActiveInputChannels().countNumberOfSetBits());
        device->setProperty("numOutputChannels", current->getActiveOutputChannels().countNumberOfSetBits());
    }
    return juce::var(device);
}

juce::var DeviceBridge::getDeviceInfo(const juce::String& deviceId)
{
    auto info = new juce::DynamicObject();
    info->setProperty("id", deviceId);
    info->setProperty("latency", 256);
    return juce::var(info);
}

// PresetBridge
PresetBridge::PresetBridge(PresetManager* mgr)
    : presetManager(mgr)
{
}

juce::var PresetBridge::savePreset(const juce::String& name, const juce::var& data)
{
    auto result = new juce::DynamicObject();
    result->setProperty("success", true);
    result->setProperty("presetName", name);
    result->setProperty("presetId", name);
    return juce::var(result);
}

juce::var PresetBridge::loadPreset(const juce::String& presetId)
{
    auto result = new juce::DynamicObject();
    result->setProperty("success", true);
    result->setProperty("presetId", presetId);
    return juce::var(result);
}

juce::var PresetBridge::listPresets()
{
    juce::var result;
    return result;
}

juce::var PresetBridge::deletePreset(const juce::String& presetId)
{
    auto result = new juce::DynamicObject();
    result->setProperty("success", true);
    result->setProperty("presetId", presetId);
    return juce::var(result);
}

// ParameterBridge
ParameterBridge::ParameterBridge(milodikfx::dsp::DSPChainManager* chain)
    : dspChain(chain)
{
}

juce::var ParameterBridge::setParameter(const juce::String& effectId, const juce::String& paramName, float value)
{
    auto result = new juce::DynamicObject();
    result->setProperty("success", true);
    result->setProperty("effectId", effectId);
    result->setProperty("paramName", paramName);
    result->setProperty("value", value);
    return juce::var(result);
}

juce::var ParameterBridge::getParameter(const juce::String& effectId, const juce::String& paramName)
{
    auto result = new juce::DynamicObject();
    result->setProperty("effectId", effectId);
    result->setProperty("paramName", paramName);
    result->setProperty("value", 0.5f);
    return juce::var(result);
}

juce::var ParameterBridge::getAllParameters()
{
    juce::var result;
    return result;
}

// EffectBridge
EffectBridge::EffectBridge(milodikfx::dsp::DSPChainManager* chain)
    : dspChain(chain)
{
}

juce::var EffectBridge::addEffect(const juce::String& effectType, int position)
{
    auto result = new juce::DynamicObject();
    result->setProperty("success", true);
    result->setProperty("effectType", effectType);
    result->setProperty("effectId", juce::Uuid().toString());
    result->setProperty("position", position);
    return juce::var(result);
}

juce::var EffectBridge::removeEffect(const juce::String& effectId)
{
    auto result = new juce::DynamicObject();
    result->setProperty("success", true);
    result->setProperty("effectId", effectId);
    return juce::var(result);
}

juce::var EffectBridge::reorderEffects(const juce::Array<juce::String>& newOrder)
{
    auto result = new juce::DynamicObject();
    result->setProperty("success", true);
    juce::var order;
    for (const auto& id : newOrder)
        order.append(id);
    result->setProperty("order", order);
    return juce::var(result);
}

juce::var EffectBridge::listEffects()
{
    juce::var result;
    return result;
}

juce::var EffectBridge::getEffectInfo(const juce::String& effectId)
{
    auto result = new juce::DynamicObject();
    result->setProperty("id", effectId);
    result->setProperty("type", "GAIN");
    result->setProperty("enabled", true);
    return juce::var(result);
}

juce::var EffectBridge::setEffectEnabled(const juce::String& effectId, bool enabled)
{
    auto result = new juce::DynamicObject();
    result->setProperty("success", true);
    result->setProperty("effectId", effectId);
    result->setProperty("enabled", enabled);
    return juce::var(result);
}

// MetricsBridge
MetricsBridge::MetricsBridge(milodikfx::audio::AudioEngine* engine)
    : audioEngine(engine)
{
}

juce::var MetricsBridge::getAudioMetrics()
{
    auto metrics = new juce::DynamicObject();
    metrics->setProperty("inputLevelDb", -20.0f);
    metrics->setProperty("outputLevelDb", -18.0f);
    metrics->setProperty("cpuLoad", 5.0f);
    metrics->setProperty("latencyMs", 10.0f);
    metrics->setProperty("peakInputLevelDb", -10.0f);
    metrics->setProperty("peakOutputLevelDb", -8.0f);
    return juce::var(metrics);
}

juce::var MetricsBridge::getCPULoad() const
{
    return 5.0f;
}

juce::var MetricsBridge::getLatency() const
{
    return 10.0f;
}

juce::var MetricsBridge::getBufferMetrics() const
{
    auto metrics = new juce::DynamicObject();
    metrics->setProperty("bufferSize", 256);
    metrics->setProperty("sampleRate", 44100);
    return juce::var(metrics);
}

} // namespace milodikfx::ipc
