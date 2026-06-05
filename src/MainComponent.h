#pragma once

#include <JuceHeader.h>

#include "audio/AudioEngine.h"
#include "dsp/GainProcessor.h"

class MainComponent final : public juce::Component,
                            private juce::AudioIODeviceCallback,
                            private juce::ChangeListener,
                            private juce::Timer
{
public:
    explicit MainComponent (juce::PropertiesFile& settingsFile);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr const char* kKeyMonitorEnabled = "ui.monitorEnabled";
    static constexpr const char* kKeyMuted = "ui.muted";
    static constexpr const char* kKeyRoutingModeId = "ui.routingModeId";
    static constexpr const char* kKeyMonitorGainDb = "ui.monitorGainDb";
    static constexpr const char* kKeyGlobalBypass = "ui.globalBypass";
    static constexpr const char* kKeyCleanBoostEnabled = "dsp.cleanBoost.enabled";
    static constexpr const char* kKeyCleanBoostGainDb = "dsp.cleanBoost.gainDb";
    static constexpr const char* kKeyAudioDeviceStateXml = "audio.deviceStateXml";

    bool initialiseAudioWithFallback (const juce::XmlElement* savedState);
    void rebuildDeviceSelector();

    void markSettingsDirty();
    void saveSettingsIfNeeded (bool force);
    std::unique_ptr<juce::XmlElement> createAudioDeviceStateXmlForPersistence() const;
    void updateAudioDeviceStateInSettings();

    // juce::AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceError (const juce::String& errorMessage) override;

    // juce::ChangeListener
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    // juce::Timer
    void timerCallback() override;

    struct LevelMeter final : public juce::Component
    {
        void setLevelsDb (float rmsDb, float peakHoldDb, bool clipped);
        void paint (juce::Graphics&) override;

        float rmsDb = -100.0f;
        float peakHoldDb = -100.0f;
        bool clipped = false;
    };

    juce::PropertiesFile& settingsFile;
    bool settingsDirty = false;
    uint32_t lastSettingsSaveMs = 0;
    uint32_t lastDeviceStatePersistTryMs = 0;

    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;
    juce::AudioBuffer<float> engineBuffer;
    milodikfx::audio::AudioEngine audioEngine;

    juce::LookAndFeel_V4 lookAndFeel;

    juce::Label titleLabel;
    juce::Label versionLabel;
    juce::Label deviceStatusLabel;

    juce::GroupComponent deviceGroup;
    juce::GroupComponent monitorGroup;
    juce::GroupComponent dspChainGroup;
    juce::GroupComponent cleanBoostGroup;

    juce::Label monitorNoteLabel;
    juce::Label inputLevelLabel;
    juce::Label outputLevelLabel;
    juce::Label dspChainNoteLabel;
    juce::Label cleanBoostGainLabel;

    juce::ToggleButton monitorEnabledToggle;
    juce::ToggleButton muteToggle;
    juce::ComboBox routingModeCombo;
    juce::Slider monitorGainSlider;
    juce::ToggleButton globalBypassToggle;
    juce::Slider cleanBoostGainSlider;
    juce::ToggleButton cleanBoostToggle;

    juce::TextButton retryAudioButton { "Retry audio" };

    LevelMeter inputMeter;
    LevelMeter outputMeter;

    juce::String audioInitError;
    juce::String audioInitNote;

    std::atomic<float> inputRms { 0.0f };
    std::atomic<float> inputPeak { 0.0f };
    std::atomic<bool> inputClipped { false };

    std::atomic<float> outputRms { 0.0f };
    std::atomic<float> outputPeak { 0.0f };
    std::atomic<bool> outputClipped { false };

    std::atomic<bool> monitorEnabled { true };
    std::atomic<bool> muted { false };
    std::atomic<int> routingMode { 1 }; // matches ComboBox selectedId
    std::atomic<float> monitorGainLinear { 1.0f };
    std::atomic<float> monitorGainDb { 0.0f };
    std::atomic<bool> globalBypass { false };
    std::atomic<float> cleanBoostGainDb { 0.0f };
    std::atomic<bool> cleanBoostEnabled { true };

    milodikfx::dsp::GainProcessor* cleanBoostProcessor = nullptr;

    float peakHoldDb = -100.0f;
    uint32_t peakHoldLastUpdateMs = 0;

    float outputPeakHoldDb = -100.0f;
    uint32_t outputPeakHoldLastUpdateMs = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
