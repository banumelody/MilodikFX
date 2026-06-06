#include "MainComponent.h"

#include <cmath>

MainComponent::MainComponent(juce::PropertiesFile& settingsFile)
    : settingsFile(settingsFile),
      presetManager(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("MilodikFX/Presets")),
      deviceManager(),
      audioEngine()
{
    // Setup device manager listener
    deviceManager.addChangeListener(this);

    // Initialize audio device with fallback
    auto error = deviceManager.initialiseWithDefaultDevices(2, 2);
    if (error.isNotEmpty())
    {
        audioInitError = error;
        audioInitNote = "Using dummy audio device";
    }
    
    deviceManager.addAudioCallback(this);

    // Setup web server to serve React frontend
    webServer = std::make_unique<WebServer>(3000);
    if (!webServer->start())
    {
        juce::Logger::getCurrentLogger()->writeToLog("Failed to start web server");
    }

    // Load settings from properties file
    cleanBoostGainDb = static_cast<float>(settingsFile.getDoubleValue(kKeyCleanBoostGainDb, 0.0));
    cleanBoostEnabled = settingsFile.getBoolValue(kKeyCleanBoostEnabled, true);
    
    overdriveDrivePct = static_cast<float>(settingsFile.getDoubleValue(kKeyOverdriveDrivePct, 0.0));
    overdriveLevelPct = static_cast<float>(settingsFile.getDoubleValue(kKeyOverdriveLevelPct, 100.0));
    overdriveEnabled = settingsFile.getBoolValue(kKeyOverdriveEnabled, true);
    
    eqBassDb = static_cast<float>(settingsFile.getDoubleValue(kKeyEqBassDb, 0.0));
    eqMidDb = static_cast<float>(settingsFile.getDoubleValue(kKeyEqMidDb, 0.0));
    eqTrebleDb = static_cast<float>(settingsFile.getDoubleValue(kKeyEqTrebleDb, 0.0));
    eqEnabled = settingsFile.getBoolValue(kKeyEqEnabled, true);
    
    compressorInputGainDb = static_cast<float>(settingsFile.getDoubleValue(kKeyCompressorInputGainDb, 0.0));
    compressorThresholdDb = static_cast<float>(settingsFile.getDoubleValue(kKeyCompressorThresholdDb, -24.0));
    compressorRatio = static_cast<float>(settingsFile.getDoubleValue(kKeyCompressorRatio, 4.0));
    compressorAttackMs = static_cast<float>(settingsFile.getDoubleValue(kKeyCompressorAttackMs, 10.0));
    compressorReleaseMs = static_cast<float>(settingsFile.getDoubleValue(kKeyCompressorReleaseMs, 100.0));
    compressorEnabled = settingsFile.getBoolValue(kKeyCompressorEnabled, true);
    
    reverbRoomSize = static_cast<float>(settingsFile.getDoubleValue(kKeyReverbRoomSize, 0.5));
    reverbDryWetMix = static_cast<float>(settingsFile.getDoubleValue(kKeyReverbDryWetMix, 0.5));
    reverbDecayTime = static_cast<float>(settingsFile.getDoubleValue(kKeyReverbDecayTime, 2.0));
    reverbWidth = static_cast<float>(settingsFile.getDoubleValue(kKeyReverbWidth, 1.0));
    reverbEnabled = settingsFile.getBoolValue(kKeyReverbEnabled, true);
    
    toneStackBassDb = static_cast<float>(settingsFile.getDoubleValue(kKeyToneStackBassDb, 0.0));
    toneStackMidDb = static_cast<float>(settingsFile.getDoubleValue(kKeyToneStackMidDb, 0.0));
    toneStackTrebleDb = static_cast<float>(settingsFile.getDoubleValue(kKeyToneStackTrebleDb, 0.0));
    toneStackEnabled = settingsFile.getBoolValue(kKeyToneStackEnabled, true);

    setSize(100, 100);
    startTimer(1000);
}

MainComponent::~MainComponent()
{
    stopTimer();
    deviceManager.removeChangeListener(this);
    deviceManager.closeAudioDevice();
    deviceManager.removeAudioCallback(this);
    if (webServer) webServer->stop();
    saveSettingsIfNeeded(true);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void MainComponent::resized()
{
}

bool MainComponent::initialiseAudioWithFallback(const juce::XmlElement*)
{
    auto error = deviceManager.initialiseWithDefaultDevices(2, 2);
    if (error.isNotEmpty())
    {
        juce::Logger::getCurrentLogger()->writeToLog("Audio Error: " + error);
        return false;
    }
    deviceManager.addAudioCallback(this);
    return true;
}

void MainComponent::markSettingsDirty()
{
    settingsDirty = true;
    lastSettingsSaveMs = juce::Time::getMillisecondCounter();
}

void MainComponent::saveSettingsIfNeeded(bool force)
{
    auto now = juce::Time::getMillisecondCounter();
    if (!settingsDirty && !force) return;
    if (!force && (now - lastSettingsSaveMs) < 2000) return;

    settingsFile.setValue(kKeyCleanBoostGainDb, static_cast<double>(cleanBoostGainDb.load()));
    settingsFile.setValue(kKeyCleanBoostEnabled, cleanBoostEnabled.load());
    settingsFile.setValue(kKeyOverdriveDrivePct, static_cast<double>(overdriveDrivePct.load()));
    settingsFile.setValue(kKeyOverdriveLevelPct, static_cast<double>(overdriveLevelPct.load()));
    settingsFile.setValue(kKeyOverdriveEnabled, overdriveEnabled.load());
    settingsFile.setValue(kKeyEqBassDb, static_cast<double>(eqBassDb.load()));
    settingsFile.setValue(kKeyEqMidDb, static_cast<double>(eqMidDb.load()));
    settingsFile.setValue(kKeyEqTrebleDb, static_cast<double>(eqTrebleDb.load()));
    settingsFile.setValue(kKeyEqEnabled, eqEnabled.load());
    settingsFile.setValue(kKeyCompressorInputGainDb, static_cast<double>(compressorInputGainDb.load()));
    settingsFile.setValue(kKeyCompressorThresholdDb, static_cast<double>(compressorThresholdDb.load()));
    settingsFile.setValue(kKeyCompressorRatio, static_cast<double>(compressorRatio.load()));
    settingsFile.setValue(kKeyCompressorAttackMs, static_cast<double>(compressorAttackMs.load()));
    settingsFile.setValue(kKeyCompressorReleaseMs, static_cast<double>(compressorReleaseMs.load()));
    settingsFile.setValue(kKeyCompressorEnabled, compressorEnabled.load());
    settingsFile.setValue(kKeyReverbRoomSize, static_cast<double>(reverbRoomSize.load()));
    settingsFile.setValue(kKeyReverbDryWetMix, static_cast<double>(reverbDryWetMix.load()));
    settingsFile.setValue(kKeyReverbDecayTime, static_cast<double>(reverbDecayTime.load()));
    settingsFile.setValue(kKeyReverbWidth, static_cast<double>(reverbWidth.load()));
    settingsFile.setValue(kKeyReverbEnabled, reverbEnabled.load());
    settingsFile.setValue(kKeyToneStackBassDb, static_cast<double>(toneStackBassDb.load()));
    settingsFile.setValue(kKeyToneStackMidDb, static_cast<double>(toneStackMidDb.load()));
    settingsFile.setValue(kKeyToneStackTrebleDb, static_cast<double>(toneStackTrebleDb.load()));
    settingsFile.setValue(kKeyToneStackEnabled, toneStackEnabled.load());
    updateAudioDeviceStateInSettings();
    settingsFile.saveIfNeeded();
    settingsDirty = false;
}

std::unique_ptr<juce::XmlElement> MainComponent::createAudioDeviceStateXmlForPersistence() const
{
    auto setup = deviceManager.getAudioDeviceSetup();
    auto xml = std::make_unique<juce::XmlElement>("AUDIO_SETUP");
    xml->setAttribute("inputDevice", setup.inputDeviceName);
    xml->setAttribute("outputDevice", setup.outputDeviceName);
    xml->setAttribute("sampleRate", juce::String(setup.sampleRate));
    xml->setAttribute("blockSize", juce::String(setup.bufferSize));
    return xml;
}

void MainComponent::updateAudioDeviceStateInSettings()
{
    auto xml = createAudioDeviceStateXmlForPersistence();
    settingsFile.setValue(kKeyAudioDeviceStateXml, xml->toString());
}

void MainComponent::audioDeviceIOCallbackWithContext(
    const float* const* inputChannelData, int numInputChannels,
    float* const* outputChannelData, int numOutputChannels,
    int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    juce::AudioBuffer<float> buffer(const_cast<float**>(inputChannelData), numInputChannels, numSamples);
    audioEngine.processBlock(buffer);

    for (int channel = 0; channel < numOutputChannels && channel < numInputChannels; ++channel)
    {
        juce::FloatVectorOperations::copy(outputChannelData[channel], buffer.getReadPointer(channel), numSamples);
    }
}

void MainComponent::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (device) audioEngine.prepareToPlay(device->getCurrentSampleRate(), 512, 2);
}

void MainComponent::audioDeviceStopped()
{
    audioEngine.reset();
}

void MainComponent::audioDeviceError(const juce::String& errorMessage)
{
    juce::Logger::getCurrentLogger()->writeToLog("Audio Device Error: " + errorMessage);
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &deviceManager) updateAudioDeviceStateInSettings();
}

void MainComponent::timerCallback()
{
    saveSettingsIfNeeded(false);
}
