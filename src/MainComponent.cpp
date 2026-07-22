#include "MainComponent.h"

#include "api/HealthHandler.h"
#include "api/UpdateHandler.h"

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

    void newWindowAttemptingToLoad (const juce::String& newURL) override
    {
        // A target=_blank link -- the sponsor page, a release -- belongs in the
        // user's real browser, not a chromeless WebView2 popup with no way back.
        juce::URL (newURL).launchInDefaultBrowser();
    }

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
                         .getChildFile ("MilodikFX/Presets")),
      irLibrary (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                     .getChildFile ("MilodikFX/ImpulseResponses")),
      namLibrary (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                      .getChildFile ("MilodikFX/NamModels"))
{
    log ("=== MainComponent constructor starting ===");

    desiredBufferSize = juce::jlimit (16, 2048, settingsFile.getIntValue (kKeyAudioBufferPreference, 128));
    desiredSampleRate = juce::jlimit (44100.0, 192000.0,
                                      settingsFile.getDoubleValue (kKeyAudioSampleRatePreference, 48000.0));
    inputMode.store (juce::jlimit (0, 3, settingsFile.getIntValue (kKeyInputMode, (int) InputMode::monoLeft)));

    buildChain();
    buildRegistry();
    loadSettingsIntoRegistry();
    buildMidi();

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

    // After the settings have been loaded, so the starting point is the chain
    // the user left rather than the factory defaults.
    undoHistory.reset();

    startTimer (1000);
    saveSettingsIfNeeded (true);

    log ("=== MainComponent constructor complete ===");
}

MainComponent::~MainComponent()
{
    stopTimer();

    // Before anything a mapped message could reach is torn down. Its callbacks
    // post to the message thread and would otherwise run against dead objects.
    midiController.openDevice ({});

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

    inputTrimProcessor = chainProcessors.inputTrim;
    noiseGateProcessor = chainProcessors.noiseGate;
    cleanBoostProcessor = chainProcessors.cleanBoost;
    compressorProcessor = chainProcessors.compressor;
    overdriveProcessor = chainProcessors.overdrive;
    eqProcessor = chainProcessors.eq;
    toneStackProcessor = chainProcessors.toneStack;
    namProcessor = chainProcessors.nam;
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
    milodikfx::dsp::ChainExtras extras;
    extras.irLibrary = &irLibrary;
    extras.namLibrary = &namLibrary;
    extras.getInputMode = [this] { return (float) inputMode.load (std::memory_order_relaxed); };
    extras.setInputMode = [this] (float v)
    {
        inputMode.store (juce::jlimit (0, 3, (int) std::lround (v)), std::memory_order_relaxed);
    };

    milodikfx::dsp::registerChainParameters (registry, chainProcessors, audioEngine.getChain(),
                                             std::move (extras));

    registry.onChanged = [this]
    {
        markSettingsDirty();

        historyDirty.store (true, std::memory_order_relaxed);
        lastParameterChangeMs.store (juce::Time::getMillisecondCounter(), std::memory_order_relaxed);
    };

    log ("Registered " + juce::String ((int) registry.getEffects().size()) + " effects");
}

//==============================================================================
void MainComponent::buildMidi()
{
    midiController.onParameterChanged = [this] { markSettingsDirty(); };

    midiController.onLearned = [this] (int, milodikfx::midi::Mapping) { markSettingsDirty(); };
    midiController.onConfigurationChanged = [this] { markSettingsDirty(); };

    // Program change selects a preset by position in the list. Names are what
    // the user thinks in, but a program number has no name to send, so the
    // ordering the preset list already has is the only mapping available.
    midiController.onProgramChange = [this] (int program)
    {
        if (presetsHandler == nullptr)
            return;

        const auto names = presetManager.listPresets();

        if (program < 0 || program >= names.size())
        {
            log ("MIDI program " + juce::String (program) + " has no preset");
            return;
        }

        const auto name = names[program];

        juce::var state;

        if (! presetManager.loadPreset (name, state))
        {
            log ("MIDI program " + juce::String (program) + ": preset " + name + " could not be read");
            return;
        }

        registry.applyState (state);
        presetsHandler->setSelectedName (name);
        markSettingsDirty();

        log ("MIDI program " + juce::String (program) + " loaded preset " + name);
    };

    loadMidiSettings();
}

void MainComponent::loadMidiSettings()
{
    for (int cc = 0; cc < milodikfx::midi::MidiController::kNumControllers; ++cc)
    {
        const auto prefix = "midi.cc." + juce::String (cc) + ".";
        const auto effect = settingsFile.getValue (prefix + "effect", {});

        if (effect.isEmpty())
            continue;

        milodikfx::midi::Mapping mapping;
        mapping.effectId = effect;
        mapping.parameterId = settingsFile.getValue (prefix + "parameter", {});
        mapping.mode = settingsFile.getValue (prefix + "mode", "continuous") == "toggle"
                           ? milodikfx::midi::MappingMode::toggle
                           : milodikfx::midi::MappingMode::continuous;

        if (mapping.isValid())
            midiController.setMapping (cc, mapping);
    }

    const auto deviceName = settingsFile.getValue (kKeyMidiDevice, {});

    if (deviceName.isEmpty())
        return;

    // A controller that was unplugged since last run is not an error worth
    // showing; the mapping stays and starts working again when it comes back.
    const auto error = midiController.openDevice (deviceName);

    if (error.isNotEmpty())
        log ("MIDI: " + error);
    else
        log ("MIDI input opened: " + deviceName);
}

void MainComponent::saveMidiSettings()
{
    settingsFile.setValue (kKeyMidiDevice, midiController.getOpenDeviceName());

    for (int cc = 0; cc < milodikfx::midi::MidiController::kNumControllers; ++cc)
    {
        const auto prefix = "midi.cc." + juce::String (cc) + ".";
        const auto mapping = midiController.getMapping (cc);

        if (! mapping.isValid())
        {
            // Removed rather than blanked, so a cleared mapping does not leave
            // an empty key that later reads back as a half-written one. Guarded
            // on containsKey because this runs for all 128 controllers on every
            // save, and removing a key that was never there would still mark
            // the file as needing a write.
            for (const auto* suffix : { "effect", "parameter", "mode" })
                if (settingsFile.containsKey (prefix + suffix))
                    settingsFile.removeValue (prefix + suffix);

            continue;
        }

        settingsFile.setValue (prefix + "effect", mapping.effectId);
        settingsFile.setValue (prefix + "parameter", mapping.parameterId);
        settingsFile.setValue (prefix + "mode",
                               mapping.mode == milodikfx::midi::MappingMode::toggle ? "toggle" : "continuous");
    }
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

    // Scenes normally travel inside a preset, but the chain has to come back the
    // way it was left even when no preset was loaded.
    const auto storedScenes = settingsFile.getValue (kKeyScenes, {});

    if (storedScenes.isNotEmpty())
    {
        juce::var parsed;

        if (juce::JSON::parse (storedScenes, parsed).wasOk() && parsed.isArray())
            sceneManager.fromVar (parsed);
    }
    else
    {
        sceneManager.resetToCurrent();
    }
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

    saveMidiSettings();

    settingsFile.setValue (kKeyScenes, juce::JSON::toString (sceneManager.toVar(), true));

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
    presetsHandler->setSceneManager (&sceneManager);
    presetsHandler->setSelectedName (settingsFile.getValue (kKeyPresetSelectedName, {}));
    presetsHandler->onSelectionChanged = [this] (const juce::String&) { markSettingsDirty(); };

    auto scenesHandler = std::make_shared<ScenesHandler> (sceneManager);
    scenesHandler->onChanged = [this] { markSettingsDirty(); };

    webServer->registerApiHandler ("/api/devices", std::make_shared<DevicesHandler> (deviceController));
    webServer->registerApiHandler ("/api/parameters",
                                   std::make_shared<ParametersHandler> (registry, "master", "volumeDb"));
    webServer->registerApiHandler ("/api/effects", std::make_shared<EffectsHandler> (registry));
    webServer->registerApiHandler ("/api/ir", std::make_shared<IrHandler> (irLibrary));
    webServer->registerApiHandler ("/api/nam", std::make_shared<NamHandler> (namLibrary));
    webServer->registerApiHandler ("/api/levels", levelsHandler);
    webServer->registerApiHandler ("/api/tuner", std::make_shared<TunerHandler> (tunerAnalyzer));
    webServer->registerApiHandler ("/api/midi", std::make_shared<MidiHandler> (midiController));
    webServer->registerApiHandler ("/api/presets", presetsHandler);
    webServer->registerApiHandler ("/api/scenes", scenesHandler);

    auto historyHandler = std::make_shared<HistoryHandler> (undoHistory, registry);
    historyHandler->onChanged = [this] { markSettingsDirty(); };
    webServer->registerApiHandler ("/api/history", historyHandler);
    webServer->registerApiHandler ("/api/health", std::make_shared<HealthHandler>());
    webServer->registerApiHandler ("/api/update", std::make_shared<UpdateHandler>());

    // Meters over one held-open connection instead of a fresh HTTP request every
    // 100 ms, which on a thread-per-connection server meant a thread per poll.
    //
    // The 33 ms asks for 30 Hz; measured delivery is about 22 Hz, because
    // Windows rounds a sleep up to the system timer granularity. Still twice
    // what the old polling managed, and chasing the last 8 Hz would mean raising
    // the global timer resolution for the sake of a meter.
    //
    // The handler is captured by shared_ptr so the stream thread cannot outlive
    // it. The server is stopped before the handler is released, but the
    // ownership should not depend on that ordering staying true.
    webServer->registerEventStream ("/api/levels/stream",
                                    [handler = levelsHandler]() -> std::string
                                    {
                                        if (handler == nullptr)
                                            return {};

                                        return handler->handleGet ("/api/levels", {}).body;
                                    },
                                    33);

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
    tunerAnalyzer.prepare (currentSampleRate);

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
        levelsHandler->updateLevels (kMeterFloorDb, kMeterFloorDb, kMeterFloorDb);
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

    // Tap the signal before the chain: the tuner has to see the raw pickup, not
    // a clipped and cabinet-filtered version of it whose harmonics would fool
    // pitch detection. A no-op unless the tuner is actually open.
    tunerAnalyzer.pushSamples (left, samples);

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
        // The trim is a pure gain applied first in the chain, so what the chain
        // receives is exactly the measured input plus the trim in dB. No second
        // measurement needed -- and without this the Input knob would be dialled
        // blind, since inputPeak is taken before the chain runs.
        const auto inputDb = juce::Decibels::gainToDecibels (inputPeak, kMeterFloorDb);
        const auto trimDb = inputTrimProcessor != nullptr ? inputTrimProcessor->getGainDb() : 0.0f;

        levels->updateLevels (inputDb,
                              juce::jmax (kMeterFloorDb, inputDb + trimDb),
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

    // Free any NAM model the audio thread swapped out. The audio thread only
    // ever moves the retired model to a slot; the reaper here deletes it off the
    // message thread, so a model switch never allocates or frees in the callback.
    if (namProcessor != nullptr)
        namProcessor->collectGarbage();

    // Commit an undo step only once the chain has been still for a moment, so
    // a knob drag is one step rather than one per frame.
    if (historyDirty.load (std::memory_order_relaxed))
    {
        const auto sinceChange = juce::Time::getMillisecondCounter()
                                 - lastParameterChangeMs.load (std::memory_order_relaxed);

        if (sinceChange >= kHistorySettleMs)
        {
            historyDirty.store (false, std::memory_order_relaxed);
            undoHistory.commitIfChanged();
        }
    }
}
