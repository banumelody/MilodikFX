#pragma once

#include <JuceHeader.h>

#include "audio/AudioEngine.h"
#include "dsp/GainProcessor.h"
#include "dsp/OverdriveProcessor.h"
#include "dsp/EQProcessor.h"
#include "dsp/CompressorProcessor.h"
#include "dsp/ReverbProcessor.h"
#include "dsp/ToneStackProcessor.h"
#include "preset/PresetManager.h"
#include "ui/WebServer.h"

class MainComponent final : public juce::Component,
                            private juce::AudioIODeviceCallback,
                            private juce::ChangeListener,
                            private juce::Timer
{
public:
    explicit MainComponent(juce::PropertiesFile& settingsFile);
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    static constexpr const char* kKeyCleanBoostEnabled = "dsp.cleanBoost.enabled";
    static constexpr const char* kKeyCleanBoostGainDb = "dsp.cleanBoost.gainDb";

    static constexpr const char* kKeyOverdriveEnabled = "dsp.overdrive.enabled";
    static constexpr const char* kKeyOverdriveDrivePct = "dsp.overdrive.drivePct";
    static constexpr const char* kKeyOverdriveLevelPct = "dsp.overdrive.levelPct";

    static constexpr const char* kKeyEqEnabled = "dsp.eq.enabled";
    static constexpr const char* kKeyEqBassDb = "dsp.eq.bassDb";
    static constexpr const char* kKeyEqMidDb = "dsp.eq.midDb";
    static constexpr const char* kKeyEqTrebleDb = "dsp.eq.trebleDb";

    static constexpr const char* kKeyCompressorEnabled = "dsp.compressor.enabled";
    static constexpr const char* kKeyCompressorInputGainDb = "dsp.compressor.inputGainDb";
    static constexpr const char* kKeyCompressorThresholdDb = "dsp.compressor.thresholdDb";
    static constexpr const char* kKeyCompressorRatio = "dsp.compressor.ratio";
    static constexpr const char* kKeyCompressorAttackMs = "dsp.compressor.attackMs";
    static constexpr const char* kKeyCompressorReleaseMs = "dsp.compressor.releaseMs";

    static constexpr const char* kKeyReverbEnabled = "dsp.reverb.enabled";
    static constexpr const char* kKeyReverbRoomSize = "dsp.reverb.roomSize";
    static constexpr const char* kKeyReverbDryWetMix = "dsp.reverb.dryWetMix";
    static constexpr const char* kKeyReverbDecayTime = "dsp.reverb.decayTime";
    static constexpr const char* kKeyReverbWidth = "dsp.reverb.width";

    static constexpr const char* kKeyToneStackEnabled = "dsp.toneStack.enabled";
    static constexpr const char* kKeyToneStackBassDb = "dsp.toneStack.bassDb";
    static constexpr const char* kKeyToneStackMidDb = "dsp.toneStack.midDb";
    static constexpr const char* kKeyToneStackTrebleDb = "dsp.toneStack.trebleDb";

    static constexpr const char* kKeyAudioDeviceStateXml = "audio.deviceStateXml";

    bool initialiseAudioWithFallback(const juce::XmlElement* savedState);

    void markSettingsDirty();
    void saveSettingsIfNeeded(bool force);
    std::unique_ptr<juce::XmlElement> createAudioDeviceStateXmlForPersistence() const;
    void updateAudioDeviceStateInSettings();

    // juce::AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                         int numInputChannels,
                                         float* const* outputChannelData,
                                         int numOutputChannels,
                                         int numSamples,
                                         const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceError(const juce::String& errorMessage) override;

    // juce::ChangeListener
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // juce::Timer
    void timerCallback() override;

    juce::PropertiesFile& settingsFile;
    milodikfx::preset::PresetManager presetManager;
    std::unique_ptr<WebServer> webServer;

    bool settingsDirty = false;
    uint32_t lastSettingsSaveMs = 0;
    uint32_t lastDeviceStatePersistTryMs = 0;

    juce::AudioDeviceManager deviceManager;
    juce::AudioBuffer<float> engineBuffer;
    milodikfx::audio::AudioEngine audioEngine;

    juce::String audioInitError;
    juce::String audioInitNote;

    std::atomic<float> cleanBoostGainDb { 0.0f };
    std::atomic<bool> cleanBoostEnabled { true };

    std::atomic<float> overdriveDrivePct { 0.0f };
    std::atomic<float> overdriveLevelPct { 100.0f };
    std::atomic<bool> overdriveEnabled { true };

    std::atomic<float> eqBassDb { 0.0f };
    std::atomic<float> eqMidDb { 0.0f };
    std::atomic<float> eqTrebleDb { 0.0f };
    std::atomic<bool> eqEnabled { true };

    std::atomic<float> compressorInputGainDb { 0.0f };
    std::atomic<float> compressorThresholdDb { -24.0f };
    std::atomic<float> compressorRatio { 4.0f };
    std::atomic<float> compressorAttackMs { 10.0f };
    std::atomic<float> compressorReleaseMs { 100.0f };
    std::atomic<bool> compressorEnabled { true };

    std::atomic<float> reverbRoomSize { 0.5f };
    std::atomic<float> reverbDryWetMix { 0.5f };
    std::atomic<float> reverbDecayTime { 2.0f };
    std::atomic<float> reverbWidth { 1.0f };
    std::atomic<bool> reverbEnabled { true };

    std::atomic<float> toneStackBassDb { 0.0f };
    std::atomic<float> toneStackMidDb { 0.0f };
    std::atomic<float> toneStackTrebleDb { 0.0f };
    std::atomic<bool> toneStackEnabled { true };

    milodikfx::dsp::GainProcessor* cleanBoostProcessor = nullptr;
    milodikfx::dsp::OverdriveProcessor* overdriveProcessor = nullptr;
    milodikfx::dsp::EQProcessor* eqProcessor = nullptr;
    milodikfx::dsp::CompressorProcessor* compressorProcessor = nullptr;
    milodikfx::dsp::ReverbProcessor* reverbProcessor = nullptr;
    milodikfx::dsp::ToneStackProcessor* toneStackProcessor = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
