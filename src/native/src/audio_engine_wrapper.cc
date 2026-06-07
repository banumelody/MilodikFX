#include "audio_engine_wrapper.h"
#include <iostream>
#include <chrono>
#include <cmath>
#include <ctime>

/**
 * AudioEngineWrapper Implementation
 * 
 * Phase 2 Day 4: Stub implementation with thread-safe atomics
 * Phase 2 Days 5-7: Will integrate with actual JUCE audio engine
 */

AudioEngineWrapper::AudioEngineWrapper()
    : initialized(false),
      audioRunning(false),
      sampleRate(44100.0),
      blockSize(512),
      numChannels(2)
{
    std::cout << "[AudioEngineWrapper] Constructor called" << std::endl;
}

AudioEngineWrapper::~AudioEngineWrapper()
{
    std::cout << "[AudioEngineWrapper] Destructor called" << std::endl;
    shutdown();
}

bool AudioEngineWrapper::initialize()
{
    std::cout << "[AudioEngineWrapper] Initializing..." << std::endl;
    
    if (initialized) {
        lastError = "Already initialized";
        return false;
    }

    // TODO Day 5: Initialize JUCE device manager
    
    initialized = true;
    lastError = "";
    
    std::cout << "[AudioEngineWrapper] Initialized successfully" << std::endl;
    return true;
}

bool AudioEngineWrapper::shutdown()
{
    std::cout << "[AudioEngineWrapper] Shutting down..." << std::endl;
    
    if (!initialized) {
        return true;
    }

    stopAudio();
    
    // TODO Day 5: Shutdown JUCE device manager
    
    initialized = false;
    return true;
}

void AudioEngineWrapper::setParameter(const std::string& effect, 
                                     const std::string& parameter, 
                                     float value)
{
    std::string key = effect + "." + parameter;
    
    // Create atomic if it doesn't exist (thread-safe)
    if (parameters.find(key) == parameters.end()) {
        parameters[key].store(value, std::memory_order_release);
    } else {
        parameters[key].store(value, std::memory_order_release);
    }
    
    std::cout << "[AudioEngineWrapper] setParameter: " << key << " = " << value << std::endl;
}

float AudioEngineWrapper::getParameter(const std::string& effect, 
                                      const std::string& parameter) const
{
    std::string key = effect + "." + parameter;
    
    auto it = parameters.find(key);
    if (it != parameters.end()) {
        return it->second.load(std::memory_order_acquire);
    }
    
    return 0.0f;
}

std::vector<AudioEngineWrapper::DeviceInfo> AudioEngineWrapper::getDeviceList() const
{
    std::cout << "[AudioEngineWrapper] getDeviceList called" << std::endl;
    
    // TODO Day 5: Query real JUCE audio devices
    // For now, return placeholder devices
    
    std::vector<DeviceInfo> devices;
    
    DeviceInfo device1;
    device1.id = "default-in";
    device1.name = "Default Input";
    device1.numInputs = 2;
    device1.numOutputs = 0;
    device1.isDefault = true;
    devices.push_back(device1);
    
    DeviceInfo device2;
    device2.id = "default-out";
    device2.name = "Default Output";
    device2.numInputs = 0;
    device2.numOutputs = 2;
    device2.isDefault = true;
    devices.push_back(device2);
    
    return devices;
}

bool AudioEngineWrapper::setDevice(const std::string& deviceId)
{
    std::cout << "[AudioEngineWrapper] setDevice: " << deviceId << std::endl;
    
    // TODO Day 5: Set real JUCE audio device
    return true;
}

std::string AudioEngineWrapper::getCurrentDevice() const
{
    // TODO Day 5: Get current device from JUCE device manager
    return "default-out";
}

AudioEngineWrapper::MeterData AudioEngineWrapper::getMeterData() const
{
    MeterData data;
    data.inputLevel = inputLevel.load(std::memory_order_acquire);
    data.outputLevel = outputLevel.load(std::memory_order_acquire);
    data.peakLeft = peakLeft.load(std::memory_order_acquire);
    data.peakRight = peakRight.load(std::memory_order_acquire);
    data.clippingDetected = clipping.load(std::memory_order_acquire);
    
    // Get current timestamp in milliseconds
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    
    return data;
}

void AudioEngineWrapper::resetMeters()
{
    inputLevel.store(-80.0f, std::memory_order_release);
    outputLevel.store(-80.0f, std::memory_order_release);
    peakLeft.store(-80.0f, std::memory_order_release);
    peakRight.store(-80.0f, std::memory_order_release);
    clipping.store(false, std::memory_order_release);
}

std::string AudioEngineWrapper::savePreset(const std::string& name, 
                                          const std::string& description)
{
    std::cout << "[AudioEngineWrapper] savePreset: " << name << std::endl;
    
    // TODO Day 7: Implement actual preset saving
    std::string presetId = "preset_" + name + "_" + std::to_string(time(nullptr));
    return presetId;
}

bool AudioEngineWrapper::loadPreset(const std::string& id)
{
    std::cout << "[AudioEngineWrapper] loadPreset: " << id << std::endl;
    
    // TODO Day 7: Implement actual preset loading
    return true;
}

std::vector<AudioEngineWrapper::PresetInfo> AudioEngineWrapper::getPresets() const
{
    std::cout << "[AudioEngineWrapper] getPresets called" << std::endl;
    
    // TODO Day 7: Return actual presets from storage
    std::vector<PresetInfo> presets;
    
    PresetInfo preset1;
    preset1.id = "preset_default";
    preset1.name = "Default";
    preset1.description = "Default preset";
    preset1.timestamp = 0;
    presets.push_back(preset1);
    
    return presets;
}

bool AudioEngineWrapper::deletePreset(const std::string& id)
{
    std::cout << "[AudioEngineWrapper] deletePreset: " << id << std::endl;
    
    // TODO Day 7: Implement actual preset deletion
    return true;
}

void AudioEngineWrapper::startAudio()
{
    std::cout << "[AudioEngineWrapper] startAudio called" << std::endl;
    
    // TODO Day 5: Start actual JUCE audio processing
    audioRunning = true;
}

void AudioEngineWrapper::stopAudio()
{
    std::cout << "[AudioEngineWrapper] stopAudio called" << std::endl;
    
    // TODO Day 5: Stop actual JUCE audio processing
    audioRunning = false;
}

float AudioEngineWrapper::calculatePeak(const float* channelData, int numSamples) const
{
    if (!channelData || numSamples <= 0) return -80.0f;
    
    float maxVal = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        maxVal = std::max(maxVal, std::abs(channelData[i]));
    }
    
    // Convert to dB (20 * log10(x))
    if (maxVal < 1e-6f) return -80.0f;
    return 20.0f * std::log10(maxVal);
}

float AudioEngineWrapper::calculateRMS(const float* channelData, int numSamples) const
{
    if (!channelData || numSamples <= 0) return -80.0f;
    
    float sum = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        float sample = channelData[i];
        sum += sample * sample;
    }
    
    float rms = std::sqrt(sum / numSamples);
    
    // Convert to dB
    if (rms < 1e-6f) return -80.0f;
    return 20.0f * std::log10(rms);
}
