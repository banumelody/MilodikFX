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

using milodikfx::api::EffectDescriptor;
using milodikfx::api::ParameterDescriptor;

ParameterDescriptor makeParam (std::string id,
                               std::string label,
                               std::string unit,
                               float minValue,
                               float maxValue,
                               float step,
                               float defaultValue,
                               std::function<float()> get,
                               std::function<void (float)> set)
{
    ParameterDescriptor p;
    p.id = std::move (id);
    p.label = std::move (label);
    p.unit = std::move (unit);
    p.minValue = minValue;
    p.maxValue = maxValue;
    p.step = step;
    p.defaultValue = defaultValue;
    p.get = std::move (get);
    p.set = std::move (set);
    return p;
}

ParameterDescriptor makeToggle (std::string id,
                                std::string label,
                                bool defaultValue,
                                std::function<bool()> get,
                                std::function<void (bool)> set)
{
    ParameterDescriptor p;
    p.id = std::move (id);
    p.label = std::move (label);
    p.minValue = 0.0f;
    p.maxValue = 1.0f;
    p.step = 1.0f;
    p.defaultValue = defaultValue ? 1.0f : 0.0f;
    p.isBoolean = true;
    p.get = [get] { return get() ? 1.0f : 0.0f; };
    p.set = [set] (float v) { set (v >= 0.5f); };
    return p;
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

        markSettingsDirty();
    };

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

    auto& chain = audioEngine.getChain();

    // Signal order matters: gate the raw pickup before anything boosts its
    // noise, compress before the clipper so the drive sees a steady level, and
    // put the cabinet after all the distortion it is supposed to be filtering.
    noiseGateProcessor = dynamic_cast<milodikfx::dsp::NoiseGateProcessor*> (
        chain.addProcessor (std::make_unique<milodikfx::dsp::NoiseGateProcessor>()));
    cleanBoostProcessor = dynamic_cast<milodikfx::dsp::GainProcessor*> (
        chain.addProcessor (std::make_unique<milodikfx::dsp::GainProcessor>()));
    compressorProcessor = dynamic_cast<milodikfx::dsp::CompressorProcessor*> (
        chain.addProcessor (std::make_unique<milodikfx::dsp::CompressorProcessor>()));
    overdriveProcessor = dynamic_cast<milodikfx::dsp::OverdriveProcessor*> (
        chain.addProcessor (std::make_unique<milodikfx::dsp::OverdriveProcessor>()));
    eqProcessor = dynamic_cast<milodikfx::dsp::EQProcessor*> (
        chain.addProcessor (std::make_unique<milodikfx::dsp::EQProcessor>()));
    toneStackProcessor = dynamic_cast<milodikfx::dsp::ToneStackProcessor*> (
        chain.addProcessor (std::make_unique<milodikfx::dsp::ToneStackProcessor>()));
    cabinetProcessor = dynamic_cast<milodikfx::dsp::CabinetProcessor*> (
        chain.addProcessor (std::make_unique<milodikfx::dsp::CabinetProcessor>()));
    delayProcessor = dynamic_cast<milodikfx::dsp::DelayProcessor*> (
        chain.addProcessor (std::make_unique<milodikfx::dsp::DelayProcessor>()));
    reverbProcessor = dynamic_cast<milodikfx::dsp::ReverbProcessor*> (
        chain.addProcessor (std::make_unique<milodikfx::dsp::ReverbProcessor>()));
    masterOutProcessor = dynamic_cast<milodikfx::dsp::MasterOutProcessor*> (
        chain.addProcessor (std::make_unique<milodikfx::dsp::MasterOutProcessor>()));
}

void MainComponent::buildRegistry()
{
    using milodikfx::api::EffectDescriptor;

    // Effect and parameter ids double as settings keys (dsp.<effect>.<param>),
    // so they are kept stable even when labels change.
    {
        EffectDescriptor e;
        e.id = "input";
        e.label = "Input";
        e.description = "How the interface's input channels feed the chain";
        e.isEnabled = [] { return true; };
        e.setEnabled = [] (bool) {};
        e.parameters.push_back (makeParam (
            "mode", "Mode", "", 0.0f, 3.0f, 1.0f, (float) InputMode::monoLeft,
            [this] { return (float) inputMode.load (std::memory_order_relaxed); },
            [this] (float v)
            {
                inputMode.store (juce::jlimit (0, 3, (int) std::lround (v)), std::memory_order_relaxed);
            }));
        registry.addEffect (std::move (e));
    }

    if (noiseGateProcessor != nullptr)
    {
        auto* p = noiseGateProcessor;
        EffectDescriptor e;
        e.id = "noiseGate";
        e.label = "Noise Gate";
        e.description = "Silences pickup hum between notes";
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };
        e.parameters.push_back (makeParam ("thresholdDb", "Threshold", "dB", -90.0f, 0.0f, 0.5f, -55.0f,
                                           [p] { return p->getThresholdDb(); },
                                           [p] (float v) { p->setThresholdDb (v); }));
        e.parameters.push_back (makeParam ("attackMs", "Attack", "ms", 0.1f, 50.0f, 0.1f, 2.0f,
                                           [p] { return p->getAttackMs(); },
                                           [p] (float v) { p->setAttackMs (v); }));
        e.parameters.push_back (makeParam ("holdMs", "Hold", "ms", 0.0f, 500.0f, 1.0f, 60.0f,
                                           [p] { return p->getHoldMs(); },
                                           [p] (float v) { p->setHoldMs (v); }));
        e.parameters.push_back (makeParam ("releaseMs", "Release", "ms", 5.0f, 1000.0f, 1.0f, 150.0f,
                                           [p] { return p->getReleaseMs(); },
                                           [p] (float v) { p->setReleaseMs (v); }));
        registry.addEffect (std::move (e));
    }

    if (cleanBoostProcessor != nullptr)
    {
        auto* p = cleanBoostProcessor;
        EffectDescriptor e;
        e.id = "cleanBoost";
        e.label = "Clean Boost";
        e.description = "Lifts a weak pickup before the rest of the chain";
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };
        e.parameters.push_back (makeParam ("gainDb", "Gain", "dB", 0.0f, 24.0f, 0.1f, 0.0f,
                                           [p] { return p->getGainDb(); },
                                           [p] (float v) { p->setGainDb (v); }));
        registry.addEffect (std::move (e));
    }

    if (compressorProcessor != nullptr)
    {
        auto* p = compressorProcessor;
        EffectDescriptor e;
        e.id = "compressor";
        e.label = "Compressor";
        e.description = "Evens out picking dynamics";
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };
        e.parameters.push_back (makeParam ("inputGainDb", "Input", "dB", -24.0f, 24.0f, 0.1f, 0.0f,
                                           [p] { return p->getInputGainDb(); },
                                           [p] (float v) { p->setInputGainDb (v); }));
        e.parameters.push_back (makeParam ("thresholdDb", "Threshold", "dB", -60.0f, 0.0f, 0.5f, -24.0f,
                                           [p] { return p->getThresholdDb(); },
                                           [p] (float v) { p->setThresholdDb (v); }));
        e.parameters.push_back (makeParam ("ratio", "Ratio", ":1", 1.0f, 20.0f, 0.1f, 4.0f,
                                           [p] { return p->getRatio(); },
                                           [p] (float v) { p->setRatio (v); }));
        e.parameters.push_back (makeParam ("attackMs", "Attack", "ms", 0.1f, 200.0f, 0.1f, 10.0f,
                                           [p] { return p->getAttackMs(); },
                                           [p] (float v) { p->setAttackMs (v); }));
        e.parameters.push_back (makeParam ("releaseMs", "Release", "ms", 5.0f, 2000.0f, 1.0f, 100.0f,
                                           [p] { return p->getReleaseMs(); },
                                           [p] (float v) { p->setReleaseMs (v); }));
        e.parameters.push_back (makeToggle ("autoMakeup", "Auto Makeup", true,
                                            [p] { return p->getAutoMakeupGain(); },
                                            [p] (bool v) { p->setAutoMakeupGain (v); }));
        registry.addEffect (std::move (e));
    }

    if (overdriveProcessor != nullptr)
    {
        auto* p = overdriveProcessor;
        EffectDescriptor e;
        e.id = "overdrive";
        e.label = "Overdrive";
        e.description = "Cubic soft clipper, oversampled";
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };
        e.parameters.push_back (makeParam ("drivePct", "Drive", "%", 0.0f, 100.0f, 0.5f, 0.0f,
                                           [p] { return p->getDrivePercent(); },
                                           [p] (float v) { p->setDrivePercent (v); }));
        e.parameters.push_back (makeParam ("levelPct", "Level", "%", 0.0f, 100.0f, 0.5f, 100.0f,
                                           [p] { return p->getLevelPercent(); },
                                           [p] (float v) { p->setLevelPercent (v); }));
        e.parameters.push_back (makeToggle ("oversampling", "Oversampling", true,
                                            [p] { return p->isOversamplingEnabled(); },
                                            [p] (bool v) { p->setOversamplingEnabled (v); }));
        registry.addEffect (std::move (e));
    }

    if (eqProcessor != nullptr)
    {
        auto* p = eqProcessor;
        EffectDescriptor e;
        e.id = "eq";
        e.label = "EQ";
        e.description = "120 Hz shelf / 1 kHz peak / 7 kHz shelf";
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };
        e.parameters.push_back (makeParam ("bassDb", "Bass", "dB", -12.0f, 12.0f, 0.1f, 0.0f,
                                           [p] { return p->getBassDb(); },
                                           [p] (float v) { p->setBassDb (v); }));
        e.parameters.push_back (makeParam ("midDb", "Mid", "dB", -12.0f, 12.0f, 0.1f, 0.0f,
                                           [p] { return p->getMidDb(); },
                                           [p] (float v) { p->setMidDb (v); }));
        e.parameters.push_back (makeParam ("trebleDb", "Treble", "dB", -12.0f, 12.0f, 0.1f, 0.0f,
                                           [p] { return p->getTrebleDb(); },
                                           [p] (float v) { p->setTrebleDb (v); }));
        registry.addEffect (std::move (e));
    }

    if (toneStackProcessor != nullptr)
    {
        auto* p = toneStackProcessor;
        EffectDescriptor e;
        e.id = "toneStack";
        e.label = "Contour";
        e.description = "50 Hz / 500 Hz / 5 kHz shaping";
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };
        e.parameters.push_back (makeParam ("bassDb", "Low", "dB", -12.0f, 12.0f, 0.1f, 0.0f,
                                           [p] { return p->getBassDb(); },
                                           [p] (float v) { p->setBassDb (v); }));
        e.parameters.push_back (makeParam ("midDb", "Mid", "dB", -12.0f, 12.0f, 0.1f, 0.0f,
                                           [p] { return p->getMidDb(); },
                                           [p] (float v) { p->setMidDb (v); }));
        e.parameters.push_back (makeParam ("trebleDb", "High", "dB", -12.0f, 12.0f, 0.1f, 0.0f,
                                           [p] { return p->getTrebleDb(); },
                                           [p] (float v) { p->setTrebleDb (v); }));
        registry.addEffect (std::move (e));
    }

    if (cabinetProcessor != nullptr)
    {
        auto* p = cabinetProcessor;
        EffectDescriptor e;
        e.id = "cabinet";
        e.label = "Cabinet";
        e.description = "Speaker emulation - leave this on for a DI'd guitar";
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };
        e.parameters.push_back (makeParam ("presenceDb", "Presence", "dB", -12.0f, 12.0f, 0.1f, 0.0f,
                                           [p] { return p->getPresenceDb(); },
                                           [p] (float v) { p->setPresenceDb (v); }));
        e.parameters.push_back (makeParam ("toneHz", "Tone", "Hz", 2000.0f, 8000.0f, 50.0f, 5500.0f,
                                           [p] { return p->getToneHz(); },
                                           [p] (float v) { p->setToneHz (v); }));
        registry.addEffect (std::move (e));
    }

    if (delayProcessor != nullptr)
    {
        auto* p = delayProcessor;
        EffectDescriptor e;
        e.id = "delay";
        e.label = "Delay";
        e.description = "Feedback delay with a gliding time control";
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };
        e.parameters.push_back (makeParam ("timeMs", "Time", "ms", 10.0f, 1000.0f, 1.0f, 350.0f,
                                           [p] { return p->getTimeMs(); },
                                           [p] (float v) { p->setTimeMs (v); }));
        e.parameters.push_back (makeParam ("feedbackPct", "Feedback", "%", 0.0f, 95.0f, 1.0f, 30.0f,
                                           [p] { return p->getFeedbackPercent(); },
                                           [p] (float v) { p->setFeedbackPercent (v); }));
        e.parameters.push_back (makeParam ("mixPct", "Mix", "%", 0.0f, 100.0f, 1.0f, 25.0f,
                                           [p] { return p->getMixPercent(); },
                                           [p] (float v) { p->setMixPercent (v); }));
        registry.addEffect (std::move (e));
    }

    if (reverbProcessor != nullptr)
    {
        auto* p = reverbProcessor;
        EffectDescriptor e;
        e.id = "reverb";
        e.label = "Reverb";
        e.description = "Freeverb-style room";
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };
        e.parameters.push_back (makeParam ("roomSize", "Size", "", 0.0f, 1.0f, 0.01f, 0.5f,
                                           [p] { return p->getRoomSize(); },
                                           [p] (float v) { p->setRoomSize (v); }));
        e.parameters.push_back (makeParam ("decayTime", "Decay", "s", 0.2f, 10.0f, 0.1f, 2.0f,
                                           [p] { return p->getDecayTime(); },
                                           [p] (float v) { p->setDecayTime (v); }));
        e.parameters.push_back (makeParam ("width", "Width", "", 0.0f, 1.0f, 0.01f, 1.0f,
                                           [p] { return p->getWidth(); },
                                           [p] (float v) { p->setWidth (v); }));
        e.parameters.push_back (makeParam ("dryWetMix", "Mix", "", 0.0f, 1.0f, 0.01f, 0.25f,
                                           [p] { return p->getDryWetMix(); },
                                           [p] (float v) { p->setDryWetMix (v); }));
        registry.addEffect (std::move (e));
    }

    if (masterOutProcessor != nullptr)
    {
        auto* p = masterOutProcessor;
        EffectDescriptor e;
        e.id = "master";
        e.label = "Master";
        e.description = "Output level and safety limiter";
        // Switching the master "off" mutes rather than bypassing, so the
        // limiter can never be taken out of the signal path.
        e.isEnabled = [p] { return ! p->isMuted(); };
        e.setEnabled = [p] (bool v) { p->setMuted (! v); };
        e.parameters.push_back (makeParam ("volumeDb", "Volume", "dB",
                                           milodikfx::dsp::MasterOutProcessor::kMinVolumeDb,
                                           milodikfx::dsp::MasterOutProcessor::kMaxVolumeDb,
                                           0.1f, 0.0f,
                                           [p] { return p->getVolumeDb(); },
                                           [p] (float v) { p->setVolumeDb (v); }));
        e.parameters.push_back (makeParam ("ceilingDb", "Ceiling", "dB", -24.0f, 0.0f, 0.1f, -0.3f,
                                           [p] { return p->getCeilingDb(); },
                                           [p] (float v) { p->setCeilingDb (v); }));
        e.parameters.push_back (makeToggle ("limiterEnabled", "Limiter", true,
                                            [p] { return p->isLimiterEnabled(); },
                                            [p] (bool v) { p->setLimiterEnabled (v); }));
        registry.addEffect (std::move (e));
    }

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

            const auto error = self->deviceController.initialise (savedState.get(),
                                                                  self->desiredBufferSize,
                                                                  self->desiredSampleRate);

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
