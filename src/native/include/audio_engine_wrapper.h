#pragma once

#include <atomic>
#include <map>
#include <string>
#include <vector>
#include <memory>

/**
 * AudioEngineWrapper - Native module wrapper for MilodikFX audio engine
 * 
 * This class bridges the Node.js/Electron frontend with the JUCE-based audio engine.
 * It provides:
 * - Audio device management
 * - Parameter control (thread-safe via atomics)
 * - Meter data streaming
 * - Preset management (stub for now)
 * 
 * Thread Safety:
 * - All parameters use std::atomic for real-time safety
 * - Meter data uses atomic updates
 * - No locks are used (lock-free design)
 * 
 * NOTE: Phase 2 Day 4: Stub implementation (no JUCE yet)
 *       Phase 2 Day 5-7: Will integrate with JUCE audio engine
 */

class AudioEngineWrapper
{
public:
    AudioEngineWrapper();
    ~AudioEngineWrapper();

    // === Initialization ===
    bool initialize();
    bool shutdown();
    bool isInitialized() const { return initialized; }

    // === Parameter Control (Real-time safe via atomics) ===
    void setParameter(const std::string& effect, 
                     const std::string& parameter, 
                     float value);
    float getParameter(const std::string& effect, 
                      const std::string& parameter) const;

    // === Device Management ===
    struct DeviceInfo {
        std::string id;
        std::string name;
        int numInputs = 0;
        int numOutputs = 0;
        bool isDefault = false;
    };

    std::vector<DeviceInfo> getDeviceList() const;
    bool setDevice(const std::string& deviceId);
    std::string getCurrentDevice() const;

    // === Meter Data (Real-time safe) ===
    struct MeterData {
        float inputLevel = -80.0f;      // dB
        float outputLevel = -80.0f;     // dB
        float peakLeft = -80.0f;        // dB
        float peakRight = -80.0f;       // dB
        float rmsLeft = -80.0f;         // dB (RMS)
        float rmsRight = -80.0f;        // dB (RMS)
        bool clippingDetected = false;
        uint64_t timestamp = 0;         // milliseconds
    };

    MeterData getMeterData() const;
    void resetMeters();

    // === Preset Management ===
    struct PresetInfo {
        std::string id;
        std::string name;
        std::string description;
        uint64_t timestamp = 0;
    };

    std::string savePreset(const std::string& name, const std::string& description);
    bool loadPreset(const std::string& id);
    std::vector<PresetInfo> getPresets() const;
    bool deletePreset(const std::string& id);

    // === Audio Stream Control ===
    void startAudio();
    void stopAudio();
    bool isAudioRunning() const { return audioRunning; }

    // === DSP Configuration ===
    int getSampleRate() const { return static_cast<int>(sampleRate); }
    int getBlockSize() const { return blockSize; }
    int getNumChannels() const { return numChannels; }

    // === Error Handling ===
    std::string getLastError() const { return lastError; }

private:
    // Audio parameters (lock-free via atomics)
    mutable std::map<std::string, std::atomic<float>> parameters;

    // Meter data (lock-free via atomics)
    mutable std::atomic<float> inputLevel { -80.0f };
    mutable std::atomic<float> outputLevel { -80.0f };
    mutable std::atomic<float> peakLeft { -80.0f };
    mutable std::atomic<float> peakRight { -80.0f };
    mutable std::atomic<bool> clipping { false };

    // State
    bool initialized = false;
    bool audioRunning = false;
    double sampleRate = 44100.0;
    int blockSize = 512;
    int numChannels = 2;

    // Error tracking
    std::string lastError;

    // Helper methods
    float calculatePeak(const float* channelData, int numSamples) const;
    float calculateRMS(const float* channelData, int numSamples) const;
};
