#pragma once

#include <JuceHeader.h>
#include "audio/AudioEngine.h"
#include "dsp/DSPChainManager.h"

namespace milodikfx::ipc
{

class DeviceBridge
{
public:
    explicit DeviceBridge(juce::AudioDeviceManager& deviceManager);

    juce::var listDevices();
    juce::var selectDevice(const juce::String& deviceId);
    juce::var getCurrentDevice();
    juce::var getDeviceInfo(const juce::String& deviceId);

private:
    juce::AudioDeviceManager& deviceManager;
};

class PresetBridge
{
public:
    explicit PresetBridge(class PresetManager* presetManager);

    juce::var savePreset(const juce::String& name, const juce::var& data);
    juce::var loadPreset(const juce::String& presetId);
    juce::var listPresets();
    juce::var deletePreset(const juce::String& presetId);

private:
    PresetManager* presetManager;
};

class ParameterBridge
{
public:
    explicit ParameterBridge(milodikfx::dsp::DSPChainManager* chain);

    juce::var setParameter(const juce::String& effectId, const juce::String& paramName, float value);
    juce::var getParameter(const juce::String& effectId, const juce::String& paramName);
    juce::var getAllParameters();

private:
    milodikfx::dsp::DSPChainManager* dspChain;
};

class EffectBridge
{
public:
    explicit EffectBridge(milodikfx::dsp::DSPChainManager* chain);

    juce::var addEffect(const juce::String& effectType, int position);
    juce::var removeEffect(const juce::String& effectId);
    juce::var reorderEffects(const juce::Array<juce::String>& newOrder);
    juce::var listEffects();
    juce::var getEffectInfo(const juce::String& effectId);
    juce::var setEffectEnabled(const juce::String& effectId, bool enabled);

private:
    milodikfx::dsp::DSPChainManager* dspChain;
};

class MetricsBridge
{
public:
    explicit MetricsBridge(class milodikfx::audio::AudioEngine* engine);

    juce::var getAudioMetrics();
    juce::var getCPULoad() const;
    juce::var getLatency() const;
    juce::var getBufferMetrics() const;

private:
    milodikfx::audio::AudioEngine* audioEngine;
};

} // namespace milodikfx::ipc
