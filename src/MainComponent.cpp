#include "MainComponent.h"

#include "api/HealthHandler.h"

#include <cmath>

namespace
{
void log (const juce::String& message)
{
    if (auto* logger = juce::Logger::getCurrentLogger())
        logger->writeToLog (message);
}

} // namespace

#if MILODIKFX_ENABLE_WEBVIEW
/**
 * The UI surface. Retries once on a network error, which covers the small
 * window where the WebView starts loading before the local server is listening.
 */
class MainComponent::UiWebView final : public juce::WebBrowserComponent,
                                       private juce::Timer
{
public:
    UiWebView (const Options& options, juce::String urlToLoad)
        : juce::WebBrowserComponent (options), url (std::move (urlToLoad))
    {
    }

    void load()
    {
        goToURL (url);
    }

    bool pageAboutToLoad (const juce::String&) override { return true; }

    void pageFinishedLoading (const juce::String&) override
    {
        retriesLeft = kMaxRetries;
        stopTimer();
    }

    bool pageLoadHadNetworkError (const juce::String& errorInfo) override
    {
        log ("WebView load error: " + errorInfo);

        if (retriesLeft-- > 0)
        {
            startTimer (400);
            return false;
        }

        return true;
    }

private:
    static constexpr int kMaxRetries = 10;

    void timerCallback() override
    {
        stopTimer();
        load();
    }

    juce::String url;
    int retriesLeft = kMaxRetries;
};
#endif

//==============================================================================
MainComponent::MainComponent (juce::PropertiesFile& settingsFileToUse)
    : settingsFile (settingsFileToUse),
      presetManager (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                         .getChildFile ("MilodikFX/Presets"))
{
    log ("=== MainComponent constructor starting ===");

    desiredBufferSize = juce::jlimit (16, 2048, settingsFile.getIntValue (kKeyAudioBufferPreference, 128));
    desiredSampleRate = juce::jlimit (44100.0, 192000.0,
                                      settingsFile.getDoubleValue (kKeyAudioSampleRatePreference, 48000.0));
    inputMode.store (juce::jlimit (0, 3, settingsFile.getIntValue (kKeyInputMode, (int) InputMode::monoLeft)));

    buildChain();
    buildRegistry();
    loadSettingsIntoRegistry();

    startServer();
    createWebView();

    setSize (1180, 760);

    deviceController.onUserRequestedSetup = [this] (double rate, int buffer)
    {
        if (rate > 0.0)
            desiredSampleRate = rate;

        if (buffer > 0)
            desiredBufferSize = buffer;

        deviceController.setPreferred (desiredSampleRate, desiredBufferSize);
        markSettingsDirty();
    };

    deviceController.setPreferred (desiredSampleRate, desiredBufferSize);

    deviceManager.addChangeListener (this);
    initialiseAudioAsync();

    startTimer (1000);
    saveSettingsIfNeeded (true);

    log ("=== MainComponent constructor complete ===");
}

MainComponent::~MainComponent()
{
    stopTimer();

   #if MILODIKFX_ENABLE_WEBVIEW
    webView.reset();
   #endif

    deviceManager.removeChangeListener (this);
    deviceManager.removeAudioCallback (this);
    deviceManager.closeAudioDevice();

    // Stop serving before the handlers (and everything they reference) go away.
    if (webServer != nullptr)
        webServer->stop();

    webServer.reset();

    saveSettingsIfNeeded (true);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0f14));
}

void MainComponent::resized()
{
   #if MILODIKFX_ENABLE_WEBVIEW
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
   #endif
}

//==============================================================================
void MainComponent::buildChain()
{
    log ("Building DSP chain...");

    chainProcessors = milodikfx::dsp::buildGuitarChain (audioEngine.getChain());

    noiseGateProcessor = chainProcessors.noiseGate;
    cleanBoostProcessor = chainProcessors.cleanBoost;
    compressorProcessor = chainProcessors.compressor;
    overdriveProcessor = chainProcessors.overdrive;
    eqProcessor = chainProcessors.eq;
    toneStackProcessor = chainProcessors.toneStack;
    cabinetProcessor = chainProcessors.cabinet;
    delayProcessor = chainProcessors.delay;
    reverbProcessor = chainProcessors.reverb;
    masterOutProcessor = chainProcessors.masterOut;
}

void MainComponent::buildRegistry()
{
    // The input stage belongs to the standalone app: it decides how the
    // interface's channels map into the chain. A plugin has no say in that, so
    // the shared factory only adds it when these accessors are supplied.
    milodikfx::dsp::registerChainParameters (
        registry,
        chainProcessors,
        audioEngine.getChain(),
        [this] { return (float) inputMode.load (std::memory_order_relaxed); },
        [this] (float v)
        {
            inputMode.store (juce::jlimit (0, 3, (int) std::lround (v)), std::memory_order_relaxed);
        });

    registry.onChanged = [this] { markSettingsDirty(); };

    log ("Registered " + juce::String ((int) registry.getEffects().size()) + " effects");
}

//==============================================================================
juce::String MainComponent::settingsKeyFor (const std::string& effectId, const std::string& parameterId) const
{
    return "dsp." + juce::String (effectId) + "." + juce::String (parameterId);
}

void MainComponent::loadSettingsIntoRegistry()
{
    for (const auto& effect : registry.getEffects())
    {
        const auto enabledKey = "dsp." + juce::String (effect.id) + ".enabled";

        if (effect.setEnabled && settingsFile.containsKey (enabledKey))
            effect.setEnabled (settingsFile.getBoolValue (enabledKey, true));

        for (const auto& parameter : effect.parameters)
        {
            const auto key = settingsKeyFor (effect.id, parameter.id);

            if (! parameter.set || ! settingsFile.containsKey (key))
                continue;

            const auto value = (float) settingsFile.getDoubleValue (key, (double) parameter.defaultValue);

            if (std::isfinite (value))
                parameter.set (juce::jlimit (parameter.minValue, parameter.maxValue, value));
        }
    }

    if (presetsHandler != nullptr)
        presetsHandler->setSelectedName (settingsFile.getValue (kKeyPresetSelectedName, {}));
}

void MainComponent::markSettingsDirty()
{
    settingsDirty = true;
}

void MainComponent::saveSettingsIfNeeded (bool force)
{
    const auto now = juce::Time::getMillisecondCounter();

    if (! settingsDirty && ! force)
        return;

    if (! force && (now - lastSettingsSaveMs) < 2000)
        return;

    for (const auto& effect : registry.getEffects())
    {
        settingsFile.setValue ("dsp." + juce::String (effect.id) + ".enabled",
                               effect.isEnabled ? effect.isEnabled() : true);

        for (const auto& parameter : effect.parameters)
            if (parameter.get)
                settingsFile.setValue (settingsKeyFor (effect.id, parameter.id), (double) parameter.get());
    }

    settingsFile.setValue (kKeyAudioBufferPreference, desiredBufferSize);
    settingsFile.setValue (kKeyAudioSampleRatePreference, desiredSampleRate);
    settingsFile.setValue (kKeyInputMode, inputMode.load (std::memory_order_relaxed));

    if (presetsHandler != nullptr)
        settingsFile.setValue (kKeyPresetSelectedName, presetsHandler->getSelectedName());

    settingsFile.saveIfNeeded();

    settingsDirty = false;
    lastSettingsSaveMs = now;
}

void MainComponent::flushSettings()
{
    persistDeviceState();
    saveSettingsIfNeeded (true);
}

void MainComponent::persistDeviceState()
{
    if (auto xml = deviceController.createStateXml())
        settingsFile.setValue (kKeyAudioDeviceStateXml, xml->toString());
}

//==============================================================================
void MainComponent::startServer()
{
    // Loopback only: this endpoint can change audio hardware and write files,
    // and there is no reason for anything off this machine to reach it.
    for (auto port = kDefaultPort; port <= kDefaultPort + 8; ++port)
    {
        auto candidate = std::make_unique<WebServer> (port);

        if (candidate->start())
        {
            webServer = std::move (candidate);
            serverPort = port;
            log ("WebServer listening on 127.0.0.1:" + juce::String (port));
            break;
        }
    }

    if (webServer == nullptr)
    {
        log ("ERROR: could not bind any port in the range "
             + juce::String (kDefaultPort) + "-" + juce::String (kDefaultPort + 8));
        return;
    }

    levelsHandler = std::make_shared<LevelsHandler>();
    presetsHandler = std::make_shared<PresetsHandler> (presetManager, registry);
    presetsHandler->setSelectedName (settingsFile.getValue (kKeyPresetSelectedName, {}));
    presetsHandler->onSelectionChanged = [this] (const juce::String&) { markSettingsDirty(); };

    webServer->registerApiHandler ("/api/devices", std::make_shared<DevicesHandler> (deviceController));
    webServer->registerApiHandler ("/api/parameters",
                                   std::make_shared<ParametersHandler> (registry, "master", "volumeDb"));
    webServer->registerApiHandler ("/api/effects", std::make_shared<EffectsHandler> (registry));
    webServer->registerApiHandler ("/api/levels", levelsHandler);
    webServer->registerApiHandler ("/api/presets", presetsHandler);
    webServer->registerApiHandler ("/api/health", std::make_shared<HealthHandler>());

    log ("REST API handlers registered");
}

void MainComponent::createWebView()
{
   #if MILODIKFX_ENABLE_WEBVIEW
    if (serverPort <= 0)
    {
        log ("Skipping WebView: no server port");
        return;
    }

    // Keep the WebView's profile out of the install directory, which may be
    // read-only under Program Files.
    auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                        .getChildFile ("MilodikFX")
                        .getChildFile ("WebView2");
    userData.createDirectory();

    juce::WebBrowserComponent::Options options;
    options = options.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2 {}
                                                  .withUserDataFolder (userData)
                                                  .withBackgroundColour (juce::Colour (0xff0d0f14))
                                                  .withStatusBarDisabled());

    webView = std::make_unique<UiWebView> (options, "http://127.0.0.1:" + juce::String (serverPort) + "/index.html");
    addAndMakeVisible (*webView);
    webView->load();

    log ("WebView2 UI attached to port " + juce::String (serverPort));
   #endif
}

void MainComponent::initialiseAudioAsync()
{
    // Opening a device can block for seconds on a bad driver, and it has to run
    // on the message thread. Deferring it keeps the window responsive, and the
    // SafePointer means a quit during startup cannot dereference a dead object.
    juce::Component::SafePointer<MainComponent> safeThis (this);

    juce::MessageManager::callAsync ([safeThis]
    {
        if (auto* self = safeThis.getComponent())
        {
            std::unique_ptr<juce::XmlElement> savedState;

            const auto stateText = self->settingsFile.getValue (kKeyAudioDeviceStateXml, {});

            if (stateText.isNotEmpty())
                savedState = juce::parseXML (stateText);

            self->deviceController.setPreferred (self->desiredSampleRate, self->desiredBufferSize);

            const auto error = self->deviceController.initialise (savedState.get());

            if (error.isNotEmpty())
                log ("Audio init failed: " + error);

            self->deviceManager.addAudioCallback (self);
            self->persistDeviceState();
        }
    });
}

//==============================================================================
void MainComponent::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (device == nullptr)
        return;

    currentSampleRate = device->getCurrentSampleRate();
    currentBlockSize = device->getCurrentBufferSizeSamples();

    log ("Audio device starting: " + device->getName()
         + " @ " + juce::String (currentSampleRate) + " Hz"
         + ", block " + juce::String (currentBlockSize)
         + ", inputs " + juce::String (device->getActiveInputChannels().countNumberOfSetBits())
         + ", outputs " + juce::String (device->getActiveOutputChannels().countNumberOfSetBits()));

    // Allocate here, never in the callback. A little headroom absorbs a driver
    // that hands us a slightly larger block than it advertised.
    engineBuffer.setSize (kMaxEngineChannels, juce::jmax (64, currentBlockSize * 2), false, true, false);
    engineBuffer.clear();

    audioEngine.prepareToPlay (currentSampleRate, engineBuffer.getNumSamples(), kMaxEngineChannels);

    audioRunning.store (true, std::memory_order_relaxed);

    if (levelsHandler != nullptr)
    {
        levelsHandler->setAudioRunning (true);
        levelsHandler->updateLoad (0.0f, currentSampleRate, currentBlockSize);
    }
}

void MainComponent::audioDeviceStopped()
{
    log ("Audio device stopped");

    audioRunning.store (false, std::memory_order_relaxed);
    audioEngine.reset();

    if (levelsHandler != nullptr)
    {
        levelsHandler->setAudioRunning (false);
        levelsHandler->updateLevels (kMeterFloorDb, kMeterFloorDb);
    }
}

void MainComponent::audioDeviceError (const juce::String& errorMessage)
{
    log ("Audio device error: " + errorMessage);
}

void MainComponent::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                      int numInputChannels,
                                                      float* const* outputChannelData,
                                                      int numOutputChannels,
                                                      int numSamples,
                                                      const juce::AudioIODeviceCallbackContext&)
{
    const juce::ScopedNoDenormals noDenormals;

    const auto startTicks = juce::Time::getHighResolutionTicks();

    if (numSamples <= 0)
        return;

    // Never process more than we allocated for; the alternative is allocating
    // on the audio thread.
    const auto samples = juce::jmin (numSamples, engineBuffer.getNumSamples());

    auto* left = engineBuffer.getWritePointer (0);
    auto* right = engineBuffer.getWritePointer (1);

    const auto* in0 = numInputChannels > 0 ? inputChannelData[0] : nullptr;
    const auto* in1 = numInputChannels > 1 ? inputChannelData[1] : nullptr;

    const auto mode = (InputMode) inputMode.load (std::memory_order_relaxed);

    if (in0 == nullptr && in1 == nullptr)
    {
        juce::FloatVectorOperations::clear (left, samples);
        juce::FloatVectorOperations::clear (right, samples);
    }
    else
    {
        switch (mode)
        {
            case InputMode::stereo:
                juce::FloatVectorOperations::copy (left, in0 != nullptr ? in0 : in1, samples);
                juce::FloatVectorOperations::copy (right, in1 != nullptr ? in1 : left, samples);
                break;

            case InputMode::monoRight:
                juce::FloatVectorOperations::copy (left, in1 != nullptr ? in1 : in0, samples);
                juce::FloatVectorOperations::copy (right, left, samples);
                break;

            case InputMode::monoSum:
                juce::FloatVectorOperations::copy (left, in0 != nullptr ? in0 : in1, samples);

                if (in0 != nullptr && in1 != nullptr)
                {
                    juce::FloatVectorOperations::add (left, in1, samples);
                    juce::FloatVectorOperations::multiply (left, 0.5f, samples);
                }

                juce::FloatVectorOperations::copy (right, left, samples);
                break;

            case InputMode::monoLeft:
            default:
                // The guitar is in input 1, so it has to reach both ears.
                juce::FloatVectorOperations::copy (left, in0 != nullptr ? in0 : in1, samples);
                juce::FloatVectorOperations::copy (right, left, samples);
                break;
        }
    }

    juce::AudioBuffer<float> view (engineBuffer.getArrayOfWritePointers(), kMaxEngineChannels, samples);

    const auto inputPeak = view.getMagnitude (0, samples);

    audioEngine.processBlock (view);

    const auto outputPeak = view.getMagnitude (0, samples);

    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        if (outputChannelData[ch] == nullptr)
            continue;

        if (ch < kMaxEngineChannels)
        {
            juce::FloatVectorOperations::copy (outputChannelData[ch], engineBuffer.getReadPointer (ch), samples);

            // Anything the driver asked for beyond what we produced stays silent.
            if (samples < numSamples)
                juce::FloatVectorOperations::clear (outputChannelData[ch] + samples, numSamples - samples);
        }
        else
        {
            juce::FloatVectorOperations::clear (outputChannelData[ch], numSamples);
        }
    }

    if (auto* levels = levelsHandler.get())
    {
        levels->updateLevels (juce::Decibels::gainToDecibels (inputPeak, kMeterFloorDb),
                              juce::Decibels::gainToDecibels (outputPeak, kMeterFloorDb));

        levels->updateGainReduction (noiseGateProcessor != nullptr ? noiseGateProcessor->getCurrentGain() : 1.0f,
                                     compressorProcessor != nullptr ? compressorProcessor->getGainReductionDb() : 0.0f,
                                     masterOutProcessor != nullptr ? masterOutProcessor->getLimiterReductionDb() : 0.0f);

        if (currentSampleRate > 0.0)
        {
            const auto elapsedSeconds = juce::Time::highResolutionTicksToSeconds (
                juce::Time::getHighResolutionTicks() - startTicks);
            const auto blockSeconds = (double) numSamples / currentSampleRate;

            if (blockSeconds > 0.0)
                levels->updateLoad ((float) (100.0 * elapsedSeconds / blockSeconds), currentSampleRate, numSamples);
        }
    }
}

//==============================================================================
void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &deviceManager)
    {
        // Persist what is open, but leave the *preferences* alone: they only
        // change when the user asks for a specific rate or buffer size.
        persistDeviceState();
        markSettingsDirty();
    }
}

void MainComponent::timerCallback()
{
    saveSettingsIfNeeded (false);
}
