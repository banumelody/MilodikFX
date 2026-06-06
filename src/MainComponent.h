#pragma once

#include <JuceHeader.h>

#include "audio/AudioEngine.h"
#include "dsp/GainProcessor.h"
#include "dsp/OverdriveProcessor.h"
#include "dsp/EQProcessor.h"
#include "preset/PresetManager.h"
#include "ui/EffectCardComponent.h"
#include "ui/LevelMeterComponent.h"
#include "ui/KnobLookAndFeelComponent.h"

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

    static constexpr const char* kKeyOverdriveEnabled = "dsp.overdrive.enabled";
    static constexpr const char* kKeyOverdriveDrivePct = "dsp.overdrive.drivePct";
    static constexpr const char* kKeyOverdriveLevelPct = "dsp.overdrive.levelPct";

    static constexpr const char* kKeyEqEnabled = "dsp.eq.enabled";
    static constexpr const char* kKeyEqBassDb = "dsp.eq.bassDb";
    static constexpr const char* kKeyEqMidDb = "dsp.eq.midDb";
    static constexpr const char* kKeyEqTrebleDb = "dsp.eq.trebleDb";

    static constexpr const char* kKeyAudioDeviceStateXml = "audio.deviceStateXml";

    static constexpr const char* kKeySelectedPresetName = "ui.preset.selectedName";

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

    struct KnobLookAndFeel final : public juce::LookAndFeel_V4
    {
        KnobLookAndFeel();

        void drawRotarySlider (juce::Graphics& g,
                              int x,
                              int y,
                              int width,
                              int height,
                              float sliderPosProportional,
                              float rotaryStartAngle,
                              float rotaryEndAngle,
                              juce::Slider& slider) override;
    };

    struct EffectCard final : public juce::Component
    {
        void setTitle (juce::String newTitle);
        void setAccentColour (juce::Colour newAccent);
        void setEnabledState (bool isEnabled);

        juce::Rectangle<int> getContentBounds() const;

        void paint (juce::Graphics& g) override;

    private:
        juce::String title;
        juce::Colour accent { juce::Colours::white };
        bool enabledState = true;
    };

    struct FootswitchButton final : public juce::ToggleButton
    {
        void setAccentColour (juce::Colour newAccent);

        void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    private:
        juce::Colour accent { juce::Colours::white };
    };

    milodikfx::preset::PresetState capturePresetState() const;
    void applyPresetState (const milodikfx::preset::PresetState& preset);
    void refreshPresetList (const juce::String& selectPresetName);

    juce::PropertiesFile& settingsFile;
    milodikfx::preset::PresetManager presetManager;

    bool settingsDirty = false;
    uint32_t lastSettingsSaveMs = 0;
    uint32_t lastDeviceStatePersistTryMs = 0;

    juce::AudioDeviceManager deviceManager;
    juce::Viewport deviceSelectorViewport;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;
    juce::AudioBuffer<float> engineBuffer;
    milodikfx::audio::AudioEngine audioEngine;

    juce::LookAndFeel_V4 lookAndFeel;
    KnobLookAndFeel knobLookAndFeel;

    juce::Label titleLabel;
    juce::Label versionLabel;
    juce::Label deviceStatusLabel;

    juce::Label presetLabel;
    juce::ComboBox presetCombo;
    juce::TextButton presetSaveButton { "Save" };
    juce::TextButton presetLoadButton { "Load" };
    juce::TextButton presetDeleteButton { "Delete" };

    juce::GroupComponent deviceGroup;
    juce::GroupComponent monitorGroup;
    juce::GroupComponent dspChainGroup;

    EffectCard cleanBoostCard;
    EffectCard overdriveCard;
    EffectCard eqCard;

    juce::Label monitorNoteLabel;
    juce::Label inputLevelLabel;
    juce::Label outputLevelLabel;
    juce::Label dspChainNoteLabel;

    juce::Label cleanBoostGainLabel;
    juce::Label cleanBoostStateLabel;

    juce::Label overdriveDriveLabel;
    juce::Label overdriveLevelLabel;
    juce::Label overdriveStateLabel;

    juce::Label eqBassLabel;
    juce::Label eqMidLabel;
    juce::Label eqTrebleLabel;
    juce::Label eqStateLabel;

    juce::ToggleButton monitorEnabledToggle;
    juce::ToggleButton muteToggle;
    juce::ComboBox routingModeCombo;
    juce::Slider monitorGainSlider;
    juce::ToggleButton globalBypassToggle;

    juce::Slider cleanBoostGainSlider;
    FootswitchButton cleanBoostToggle;

    juce::Slider overdriveDriveSlider;
    juce::Slider overdriveLevelSlider;
    FootswitchButton overdriveToggle;

    juce::Slider eqBassSlider;
    juce::Slider eqMidSlider;
    juce::Slider eqTrebleSlider;
    FootswitchButton eqToggle;

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

    std::atomic<float> overdriveDrivePct { 0.0f };
    std::atomic<float> overdriveLevelPct { 100.0f };
    std::atomic<bool> overdriveEnabled { true };

    std::atomic<float> eqBassDb { 0.0f };
    std::atomic<float> eqMidDb { 0.0f };
    std::atomic<float> eqTrebleDb { 0.0f };
    std::atomic<bool> eqEnabled { true };

    milodikfx::dsp::GainProcessor* cleanBoostProcessor = nullptr;
    milodikfx::dsp::OverdriveProcessor* overdriveProcessor = nullptr;
    milodikfx::dsp::EQProcessor* eqProcessor = nullptr;

    float peakHoldDb = -100.0f;
    uint32_t peakHoldLastUpdateMs = 0;

    float outputPeakHoldDb = -100.0f;
    uint32_t outputPeakHoldLastUpdateMs = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
