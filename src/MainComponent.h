#pragma once

#include <JuceHeader.h>

#include <memory>

#include "api/DevicesHandler.h"
#include "api/EffectsHandler.h"
#include "api/IrHandler.h"
#include "api/LevelsHandler.h"
#include "api/ParameterRegistry.h"
#include "api/ParametersHandler.h"
#include "api/PresetsHandler.h"
#include "api/TunerHandler.h"
#include "audio/AudioDeviceController.h"
#include "audio/AudioEngine.h"
#include "dsp/ChainFactory.h"
#include "dsp/TunerAnalyzer.h"
#include "preset/IrLibrary.h"
#include "preset/PresetManager.h"
#include "ui/WebServer.h"

/**
 * The composition root and the window's content component.
 *
 * Owns the DSP chain, the parameter registry, the local HTTP server, and the
 * WebView that renders the UI. Construction order matters: the server and the
 * WebView come up first so the window is usable immediately, and the audio
 * device is opened afterwards from a message-thread callback.
 */
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

    int getServerPort() const noexcept { return serverPort; }

    /** Writes any pending settings immediately (called on quit). */
    void flushSettings();

    /** How the device's input channels are mapped into the stereo chain. */
    enum class InputMode
    {
        monoLeft = 0,   // guitar in input 1, sent to both ears
        monoRight = 1,
        monoSum = 2,
        stereo = 3
    };

private:
    static constexpr float kMeterFloorDb = -100.0f;
    static constexpr int kMaxEngineChannels = 2;
    static constexpr int kDefaultPort = 3000;

    static constexpr const char* kKeyAudioDeviceStateXml = "audio.deviceStateXml";
    static constexpr const char* kKeyAudioBufferPreference = "audio.bufferSizePreference";
    static constexpr const char* kKeyAudioSampleRatePreference = "audio.sampleRatePreference";
    static constexpr const char* kKeyPresetSelectedName = "ui.preset.selectedName";
    static constexpr const char* kKeyInputMode = "audio.inputMode";

    void buildChain();
    void buildRegistry();
    void startServer();
    void createWebView();
    void initialiseAudioAsync();

    void loadSettingsIntoRegistry();
    void markSettingsDirty();
    void saveSettingsIfNeeded (bool force);
    void persistDeviceState();

    juce::String settingsKeyFor (const std::string& effectId, const std::string& parameterId) const;

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

    juce::PropertiesFile& settingsFile;
    milodikfx::preset::PresetManager presetManager;
    milodikfx::preset::IrLibrary irLibrary;
    milodikfx::api::ParameterRegistry registry;

    juce::AudioDeviceManager deviceManager;
    milodikfx::audio::AudioDeviceController deviceController { deviceManager };
    milodikfx::audio::AudioEngine audioEngine;

    std::unique_ptr<WebServer> webServer;
    int serverPort = 0;

    std::shared_ptr<LevelsHandler> levelsHandler;
    std::shared_ptr<PresetsHandler> presetsHandler;

   #if MILODIKFX_ENABLE_WEBVIEW
    class UiWebView;
    std::unique_ptr<UiWebView> webView;
   #endif

    // Audio-thread state.
    juce::AudioBuffer<float> engineBuffer;
    std::atomic<int> inputMode { (int) InputMode::monoLeft };
    std::atomic<bool> audioRunning { false };
    double currentSampleRate = 0.0;
    int currentBlockSize = 0;

    bool settingsDirty = false;
    uint32_t lastSettingsSaveMs = 0;
    int desiredBufferSize = 128;
    double desiredSampleRate = 48000.0;

    milodikfx::dsp::GuitarChain chainProcessors;

    milodikfx::dsp::NoiseGateProcessor* noiseGateProcessor = nullptr;
    milodikfx::dsp::GainProcessor* cleanBoostProcessor = nullptr;
    milodikfx::dsp::CompressorProcessor* compressorProcessor = nullptr;
    milodikfx::dsp::OverdriveProcessor* overdriveProcessor = nullptr;
    milodikfx::dsp::EQProcessor* eqProcessor = nullptr;
    milodikfx::dsp::ToneStackProcessor* toneStackProcessor = nullptr;
    milodikfx::dsp::CabinetProcessor* cabinetProcessor = nullptr;
    milodikfx::dsp::DelayProcessor* delayProcessor = nullptr;
    milodikfx::dsp::ReverbProcessor* reverbProcessor = nullptr;
    milodikfx::dsp::MasterOutProcessor* masterOutProcessor = nullptr;

    // Analyses the post-input signal without being part of the chain, so it
    // reads what the pickup sends rather than what the overdrive makes of it.
    milodikfx::dsp::TunerAnalyzer tunerAnalyzer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
