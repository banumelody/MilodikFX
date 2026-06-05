#include "MainComponent.h"

#include <cmath>

static float rmsToDb (float rms)
{
    return juce::Decibels::gainToDecibels (rms, -100.0f);
}

MainComponent::KnobLookAndFeel::KnobLookAndFeel()
{
    setColourScheme (juce::LookAndFeel_V4::getDarkColourScheme());
}

void MainComponent::KnobLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                                      int x,
                                                      int y,
                                                      int width,
                                                      int height,
                                                      float sliderPosProportional,
                                                      float rotaryStartAngle,
                                                      float rotaryEndAngle,
                                                      juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (6.0f);
    const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());

    const auto radius = (size * 0.5f) - 2.0f;
    const auto cx = bounds.getCentreX();
    const auto cy = bounds.getCentreY();

    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    const auto fill = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const auto outline = slider.findColour (juce::Slider::rotarySliderOutlineColourId);

    const auto knobArea = juce::Rectangle<float> (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    // Knob body
    {
        juce::ColourGradient grad (juce::Colour (0xff2c2f33), cx, cy - radius,
                                  juce::Colour (0xff0f1113), cx, cy + radius, false);
        g.setGradientFill (grad);
        g.fillEllipse (knobArea);

        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawEllipse (knobArea, 1.5f);

        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (knobArea.reduced (2.0f), 1.0f);
    }

    // Arc ring
    {
        const auto arcRadius = radius + 3.0f;
        const auto arcThickness = 4.0f;

        juce::Path bgArc;
        bgArc.addCentredArc (cx, cy, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (outline.withAlpha (0.35f));
        g.strokePath (bgArc, juce::PathStrokeType (arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc (cx, cy, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
        g.setColour (fill);
        g.strokePath (valueArc, juce::PathStrokeType (arcThickness + 0.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Pointer
    {
        const auto pointerLen = radius * 0.75f;
        const auto pointerThickness = 2.0f;

        const auto dx = std::cos (angle - juce::MathConstants<float>::halfPi);
        const auto dy = std::sin (angle - juce::MathConstants<float>::halfPi);

        const auto x2 = cx + pointerLen * dx;
        const auto y2 = cy + pointerLen * dy;

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawLine (cx, cy, x2, y2, pointerThickness);

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (juce::Rectangle<float> (cx - 3.5f, cy - 3.5f, 7.0f, 7.0f));
    }
}

void MainComponent::LevelMeter::setLevelsDb (float newRmsDb, float newPeakHoldDb, bool isClipped)
{
    rmsDb = juce::jlimit (-100.0f, 0.0f, newRmsDb);
    peakHoldDb = juce::jlimit (-100.0f, 0.0f, newPeakHoldDb);
    clipped = isClipped;
    repaint();
}

void MainComponent::LevelMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (b, 4.0f);

    const auto rmsNorm = juce::jmap (rmsDb, -60.0f, 0.0f, 0.0f, 1.0f);
    const auto rmsClamped = juce::jlimit (0.0f, 1.0f, rmsNorm);

    auto fill = b;
    fill.removeFromTop (b.getHeight() * (1.0f - rmsClamped));

    g.setColour (rmsDb > -6.0f ? juce::Colours::orange : juce::Colours::limegreen);
    g.fillRoundedRectangle (fill.reduced (3.0f), 3.0f);

    // Peak-hold marker
    const auto peakNorm = juce::jmap (peakHoldDb, -60.0f, 0.0f, 0.0f, 1.0f);
    const auto peakClamped = juce::jlimit (0.0f, 1.0f, peakNorm);
    const auto y = b.getBottom() - (b.getHeight() * peakClamped);

    g.setColour (juce::Colours::yellow);
    g.drawLine (b.getX() + 3.0f, y, b.getRight() - 3.0f, y, 2.0f);

    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawRoundedRectangle (b, 4.0f, 1.0f);

    if (clipped)
    {
        g.setColour (juce::Colours::red.withAlpha (0.9f));
        g.drawRoundedRectangle (b.reduced (1.0f), 4.0f, 2.0f);
        g.setFont (12.0f);
        g.drawFittedText ("CLIP", getLocalBounds().reduced (4), juce::Justification::centredTop, 1);
    }
}

bool MainComponent::initialiseAudioWithFallback (const juce::XmlElement* savedState)
{
    deviceManager.removeAudioCallback (this);
    deviceManager.closeAudioDevice();

    inputRms.store (0.0f, std::memory_order_relaxed);
    inputPeak.store (0.0f, std::memory_order_relaxed);
    inputClipped.store (false, std::memory_order_relaxed);

    outputRms.store (0.0f, std::memory_order_relaxed);
    outputPeak.store (0.0f, std::memory_order_relaxed);
    outputClipped.store (false, std::memory_order_relaxed);

    peakHoldDb = -100.0f;
    peakHoldLastUpdateMs = 0;

    outputPeakHoldDb = -100.0f;
    outputPeakHoldLastUpdateMs = 0;

    audioInitError.clear();
    audioInitNote.clear();

    const juce::XmlElement* stateToUse = savedState;
    bool asioAvailable = false;

    for (auto* t : deviceManager.getAvailableDeviceTypes())
    {
        if (t != nullptr && t->getTypeName().equalsIgnoreCase ("ASIO"))
        {
            asioAvailable = ! t->getDeviceNames (true).isEmpty() || ! t->getDeviceNames (false).isEmpty();

            if (asioAvailable)
                deviceManager.setCurrentAudioDeviceType (t->getTypeName(), false);

            break;
        }
    }

    if (asioAvailable && savedState != nullptr)
    {
        const auto typeAttr = savedState->getStringAttribute ("deviceType");
        if (! typeAttr.equalsIgnoreCase ("ASIO"))
            stateToUse = nullptr;
    }

    auto err = deviceManager.initialise (2, 2, stateToUse, true);

    // If the saved state is invalid for this machine, fall back to defaults.
    if (stateToUse != nullptr && err.isNotEmpty())
        err = deviceManager.initialise (2, 2, nullptr, true);

    if (err.isEmpty())
    {
        deviceManager.addAudioCallback (this);
        return true;
    }

    // Fallback: keep the app usable even if there's no input device.
    auto fallbackErr = deviceManager.initialise (0, 2, nullptr, true);
    if (fallbackErr.isEmpty())
    {
        audioInitNote = "No input channels available; running output-only.";
        deviceManager.addAudioCallback (this);
        return true;
    }

    audioInitError = "2in/2out failed: " + err + " | 0in/2out failed: " + fallbackErr;
    return false;
}

void MainComponent::rebuildDeviceSelector()
{
    if (deviceSelector != nullptr)
        removeChildComponent (deviceSelector.get());

    deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent> (
        deviceManager,
        0, 2,   // min/max inputs
        1, 2,   // min/max outputs
        true,   // show MIDI input
        false,  // show MIDI output
        true,   // show channels
        true    // show advanced
    );

    addAndMakeVisible (*deviceSelector);
}

void MainComponent::markSettingsDirty()
{
    settingsDirty = true;
}

void MainComponent::saveSettingsIfNeeded (bool force)
{
    if (! settingsDirty)
        return;

    const auto now = juce::Time::getMillisecondCounter();

    if (! force && lastSettingsSaveMs != 0 && now - lastSettingsSaveMs <= 1000)
        return;

    const auto ok = force ? settingsFile.save()
                          : settingsFile.saveIfNeeded();

    if (ok)
    {
        lastSettingsSaveMs = now;
        settingsDirty = false;
    }
    // If saving fails, keep settingsDirty = true so we can retry later.
}

std::unique_ptr<juce::XmlElement> MainComponent::createAudioDeviceStateXmlForPersistence() const
{
    auto xml = std::make_unique<juce::XmlElement> ("DEVICESETUP");

    auto setup = deviceManager.getAudioDeviceSetup();
    xml->setAttribute ("deviceType", deviceManager.getCurrentAudioDeviceType());
    xml->setAttribute ("audioOutputDeviceName", setup.outputDeviceName);
    xml->setAttribute ("audioInputDeviceName", setup.inputDeviceName);

    if (auto* dev = deviceManager.getCurrentAudioDevice())
    {
        xml->setAttribute ("audioDeviceRate", dev->getCurrentSampleRate());

        if (dev->getDefaultBufferSize() != dev->getCurrentBufferSizeSamples())
            xml->setAttribute ("audioDeviceBufferSize", dev->getCurrentBufferSizeSamples());

        if (! setup.useDefaultInputChannels)
            xml->setAttribute ("audioDeviceInChans", setup.inputChannels.toString (2));

        if (! setup.useDefaultOutputChannels)
            xml->setAttribute ("audioDeviceOutChans", setup.outputChannels.toString (2));
    }

    return xml;
}

void MainComponent::updateAudioDeviceStateInSettings()
{
    auto xml = deviceManager.createStateXml();

    if (xml == nullptr)
        xml = createAudioDeviceStateXmlForPersistence();

    if (xml != nullptr)
    {
        settingsFile.setValue (kKeyAudioDeviceStateXml, xml->toString());
        markSettingsDirty();
    }
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateAudioDeviceStateInSettings();
}

MainComponent::MainComponent (juce::PropertiesFile& settings)
    : settingsFile (settings)
{
    bool persistedMonitor = true;
    bool persistedMute = false;
    int persistedRouting = 1;
    double persistedGainDb = 0.0;
    bool persistedGlobalBypass = false;

    double persistedCleanBoostGainDb = 0.0;
    bool persistedCleanBoostEnabled = true;

    double persistedOverdriveDrivePct = 0.0;
    double persistedOverdriveLevelPct = 100.0;
    bool persistedOverdriveEnabled = true;

    juce::String persistedAudioXml;

    persistedMonitor = settingsFile.getBoolValue (kKeyMonitorEnabled, true);
    persistedMute = settingsFile.getBoolValue (kKeyMuted, false);
    persistedRouting = settingsFile.getIntValue (kKeyRoutingModeId, 1);
    persistedGainDb = settingsFile.getDoubleValue (kKeyMonitorGainDb, 0.0);
    persistedGlobalBypass = settingsFile.getBoolValue (kKeyGlobalBypass, false);

    persistedCleanBoostGainDb = settingsFile.getDoubleValue (kKeyCleanBoostGainDb, 0.0);
    persistedCleanBoostEnabled = settingsFile.getBoolValue (kKeyCleanBoostEnabled, true);

    persistedOverdriveDrivePct = settingsFile.getDoubleValue (kKeyOverdriveDrivePct, 0.0);
    persistedOverdriveLevelPct = settingsFile.getDoubleValue (kKeyOverdriveLevelPct, 100.0);
    persistedOverdriveEnabled = settingsFile.getBoolValue (kKeyOverdriveEnabled, true);

    persistedAudioXml = settingsFile.getValue (kKeyAudioDeviceStateXml);

    persistedRouting = juce::jlimit (1, 4, persistedRouting);
    persistedGainDb = juce::jlimit (-24.0, 12.0, persistedGainDb);
    persistedCleanBoostGainDb = juce::jlimit (0.0, 24.0, persistedCleanBoostGainDb);

    persistedOverdriveDrivePct = juce::jlimit (0.0, 100.0, persistedOverdriveDrivePct);
    persistedOverdriveLevelPct = juce::jlimit (0.0, 100.0, persistedOverdriveLevelPct);

    monitorEnabled.store (persistedMonitor, std::memory_order_relaxed);
    muted.store (persistedMute, std::memory_order_relaxed);
    routingMode.store (persistedRouting, std::memory_order_relaxed);
    monitorGainDb.store ((float) persistedGainDb, std::memory_order_relaxed);
    monitorGainLinear.store (juce::Decibels::decibelsToGain ((float) persistedGainDb), std::memory_order_relaxed);

    globalBypass.store (persistedGlobalBypass, std::memory_order_relaxed);
    audioEngine.setBypassed (persistedGlobalBypass);

    cleanBoostGainDb.store ((float) persistedCleanBoostGainDb, std::memory_order_relaxed);
    cleanBoostEnabled.store (persistedCleanBoostEnabled, std::memory_order_relaxed);

    overdriveDrivePct.store ((float) persistedOverdriveDrivePct, std::memory_order_relaxed);
    overdriveLevelPct.store ((float) persistedOverdriveLevelPct, std::memory_order_relaxed);
    overdriveEnabled.store (persistedOverdriveEnabled, std::memory_order_relaxed);

    if (auto* processor = audioEngine.getChain().addProcessor (std::make_unique<milodikfx::dsp::GainProcessor>()))
        cleanBoostProcessor = dynamic_cast<milodikfx::dsp::GainProcessor*> (processor);

    if (auto* processor = audioEngine.getChain().addProcessor (std::make_unique<milodikfx::dsp::OverdriveProcessor>()))
        overdriveProcessor = dynamic_cast<milodikfx::dsp::OverdriveProcessor*> (processor);

    if (cleanBoostProcessor != nullptr)
    {
        cleanBoostProcessor->setGainDb ((float) persistedCleanBoostGainDb);
        cleanBoostProcessor->setEnabled (persistedCleanBoostEnabled);
    }

    if (overdriveProcessor != nullptr)
    {
        overdriveProcessor->setDrivePercent ((float) persistedOverdriveDrivePct);
        overdriveProcessor->setLevelPercent ((float) persistedOverdriveLevelPct);
        overdriveProcessor->setEnabled (persistedOverdriveEnabled);
    }

    std::unique_ptr<juce::XmlElement> persistedAudioState;
    if (persistedAudioXml.isNotEmpty())
        persistedAudioState = juce::parseXML (persistedAudioXml);

    // Seed settings early (before potentially slow audio init) so the file is created promptly.
    settingsFile.setValue (kKeyMonitorEnabled, persistedMonitor);
    settingsFile.setValue (kKeyMuted, persistedMute);
    settingsFile.setValue (kKeyRoutingModeId, persistedRouting);
    settingsFile.setValue (kKeyMonitorGainDb, persistedGainDb);
    settingsFile.setValue (kKeyGlobalBypass, persistedGlobalBypass);
    settingsFile.setValue (kKeyCleanBoostGainDb, persistedCleanBoostGainDb);
    settingsFile.setValue (kKeyCleanBoostEnabled, persistedCleanBoostEnabled);
    settingsFile.setValue (kKeyOverdriveDrivePct, persistedOverdriveDrivePct);
    settingsFile.setValue (kKeyOverdriveLevelPct, persistedOverdriveLevelPct);
    settingsFile.setValue (kKeyOverdriveEnabled, persistedOverdriveEnabled);
    markSettingsDirty();
    saveSettingsIfNeeded (true);

    lookAndFeel.setColourScheme (juce::LookAndFeel_V4::getDarkColourScheme());
    setLookAndFeel (&lookAndFeel);

    titleLabel.setText ("MilodikFX", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    versionLabel.setText ("v" + juce::String (ProjectInfo::versionString), juce::dontSendNotification);
    versionLabel.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::plain)));
    versionLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    versionLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (versionLabel);

    deviceStatusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (deviceStatusLabel);

    deviceGroup.setText ("Audio Device");
    addAndMakeVisible (deviceGroup);

    monitorGroup.setText ("Monitoring");
    addAndMakeVisible (monitorGroup);

    dspChainGroup.setText ("DSP Chain");
    addAndMakeVisible (dspChainGroup);

    cleanBoostGroup.setText ("Clean Boost");
    cleanBoostGroup.setColour (juce::GroupComponent::textColourId, juce::Colour (0xff6ee04a));
    cleanBoostGroup.setColour (juce::GroupComponent::outlineColourId, juce::Colour (0xff6ee04a).withAlpha (0.35f));
    addAndMakeVisible (cleanBoostGroup);

    overdriveGroup.setText ("Overdrive");
    overdriveGroup.setColour (juce::GroupComponent::textColourId, juce::Colour (0xffff9a1f));
    overdriveGroup.setColour (juce::GroupComponent::outlineColourId, juce::Colour (0xffff9a1f).withAlpha (0.35f));
    addAndMakeVisible (overdriveGroup);

    monitorNoteLabel.setText ("Passthrough: input -> output", juce::dontSendNotification);
    monitorNoteLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (monitorNoteLabel);

    inputLevelLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (inputLevelLabel);

    outputLevelLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (outputLevelLabel);

    dspChainNoteLabel.setText ("DSP Chain: Clean Boost -> Overdrive", juce::dontSendNotification);
    dspChainNoteLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (dspChainNoteLabel);

    cleanBoostGainLabel.setText ("Gain", juce::dontSendNotification);
    cleanBoostGainLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (cleanBoostGainLabel);

    overdriveDriveLabel.setText ("Drive", juce::dontSendNotification);
    overdriveDriveLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (overdriveDriveLabel);

    overdriveLevelLabel.setText ("Level", juce::dontSendNotification);
    overdriveLevelLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (overdriveLevelLabel);

    monitorEnabledToggle.setButtonText ("Monitor");
    monitorEnabledToggle.setToggleState (persistedMonitor, juce::dontSendNotification);
    monitorEnabledToggle.onClick = [this]
    {
        const auto v = monitorEnabledToggle.getToggleState();
        monitorEnabled.store (v, std::memory_order_relaxed);

        settingsFile.setValue (kKeyMonitorEnabled, v);
        markSettingsDirty();
    };
    addAndMakeVisible (monitorEnabledToggle);

    muteToggle.setButtonText ("Mute");
    muteToggle.setToggleState (persistedMute, juce::dontSendNotification);
    muteToggle.onClick = [this]
    {
        const auto v = muteToggle.getToggleState();
        muted.store (v, std::memory_order_relaxed);

        settingsFile.setValue (kKeyMuted, v);
        markSettingsDirty();
    };
    addAndMakeVisible (muteToggle);

    routingModeCombo.addItem ("Match channels", 1);
    routingModeCombo.addItem ("Mono -> Stereo", 2);
    routingModeCombo.addItem ("Stereo -> Mono (to all outs)", 3);
    routingModeCombo.addItem ("Stereo -> Mono (L only)", 4);
    routingModeCombo.setSelectedId (persistedRouting, juce::dontSendNotification);
    routingModeCombo.onChange = [this]
    {
        const auto id = routingModeCombo.getSelectedId();
        routingMode.store (id, std::memory_order_relaxed);

        settingsFile.setValue (kKeyRoutingModeId, id);
        markSettingsDirty();
    };
    addAndMakeVisible (routingModeCombo);

    monitorGainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    monitorGainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 20);
    monitorGainSlider.setTextValueSuffix (" dB");
    monitorGainSlider.setRange (-24.0, 12.0, 0.1);
    monitorGainSlider.setValue (persistedGainDb, juce::dontSendNotification);
    monitorGainSlider.onValueChange = [this]
    {
        const auto db = (float) monitorGainSlider.getValue();
        monitorGainDb.store (db, std::memory_order_relaxed);
        monitorGainLinear.store (juce::Decibels::decibelsToGain (db), std::memory_order_relaxed);

        settingsFile.setValue (kKeyMonitorGainDb, (double) db);
        markSettingsDirty();
    };
    addAndMakeVisible (monitorGainSlider);

    globalBypassToggle.setButtonText ("Global Bypass");
    globalBypassToggle.setToggleState (persistedGlobalBypass, juce::dontSendNotification);
    globalBypassToggle.onClick = [this]
    {
        const auto v = globalBypassToggle.getToggleState();
        globalBypass.store (v, std::memory_order_relaxed);
        audioEngine.setBypassed (v);

        settingsFile.setValue (kKeyGlobalBypass, v);
        markSettingsDirty();
    };
    addAndMakeVisible (globalBypassToggle);

    auto configureKnob = [this] (juce::Slider& s, juce::Colour accent)
    {
        s.setLookAndFeel (&knobLookAndFeel);
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        s.setColour (juce::Slider::rotarySliderFillColourId, accent);
        s.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::white.withAlpha (0.22f));
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff0f1113));
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::black.withAlpha (0.65f));
        s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.9f));
    };

    auto stylePowerToggle = [] (juce::ToggleButton& b, juce::Colour onColour)
    {
        b.setButtonText (b.getToggleState() ? "ON" : "OFF");
        b.setColour (juce::ToggleButton::textColourId,
                     b.getToggleState() ? onColour : juce::Colours::white.withAlpha (0.5f));
    };

    configureKnob (cleanBoostGainSlider, juce::Colour (0xff6ee04a));
    cleanBoostGainSlider.setTextValueSuffix (" dB");
    cleanBoostGainSlider.setRange (0.0, 24.0, 0.1);
    cleanBoostGainSlider.setValue (persistedCleanBoostGainDb, juce::dontSendNotification);
    cleanBoostGainSlider.onValueChange = [this]
    {
        const auto db = (float) cleanBoostGainSlider.getValue();
        cleanBoostGainDb.store (db, std::memory_order_relaxed);
        if (cleanBoostProcessor != nullptr)
            cleanBoostProcessor->setGainDb (db);

        settingsFile.setValue (kKeyCleanBoostGainDb, (double) db);
        markSettingsDirty();
    };
    addAndMakeVisible (cleanBoostGainSlider);

    cleanBoostToggle.setToggleState (persistedCleanBoostEnabled, juce::dontSendNotification);
    stylePowerToggle (cleanBoostToggle, juce::Colour (0xff6ee04a));
    cleanBoostToggle.onClick = [this, stylePowerToggle]
    {
        const auto v = cleanBoostToggle.getToggleState();
        cleanBoostEnabled.store (v, std::memory_order_relaxed);
        if (cleanBoostProcessor != nullptr)
            cleanBoostProcessor->setEnabled (v);

        settingsFile.setValue (kKeyCleanBoostEnabled, v);
        markSettingsDirty();
        stylePowerToggle (cleanBoostToggle, juce::Colour (0xff6ee04a));
    };
    addAndMakeVisible (cleanBoostToggle);

    configureKnob (overdriveDriveSlider, juce::Colour (0xffff9a1f));
    overdriveDriveSlider.setTextValueSuffix (" %");
    overdriveDriveSlider.setNumDecimalPlacesToDisplay (0);
    overdriveDriveSlider.setRange (0.0, 100.0, 1.0);
    overdriveDriveSlider.setValue (persistedOverdriveDrivePct, juce::dontSendNotification);
    overdriveDriveSlider.onValueChange = [this]
    {
        const auto pct = (float) overdriveDriveSlider.getValue();
        overdriveDrivePct.store (pct, std::memory_order_relaxed);
        if (overdriveProcessor != nullptr)
            overdriveProcessor->setDrivePercent (pct);

        settingsFile.setValue (kKeyOverdriveDrivePct, (double) pct);
        markSettingsDirty();
    };
    addAndMakeVisible (overdriveDriveSlider);

    configureKnob (overdriveLevelSlider, juce::Colour (0xffff9a1f));
    overdriveLevelSlider.setTextValueSuffix (" %");
    overdriveLevelSlider.setNumDecimalPlacesToDisplay (0);
    overdriveLevelSlider.setRange (0.0, 100.0, 1.0);
    overdriveLevelSlider.setValue (persistedOverdriveLevelPct, juce::dontSendNotification);
    overdriveLevelSlider.onValueChange = [this]
    {
        const auto pct = (float) overdriveLevelSlider.getValue();
        overdriveLevelPct.store (pct, std::memory_order_relaxed);
        if (overdriveProcessor != nullptr)
            overdriveProcessor->setLevelPercent (pct);

        settingsFile.setValue (kKeyOverdriveLevelPct, (double) pct);
        markSettingsDirty();
    };
    addAndMakeVisible (overdriveLevelSlider);

    overdriveToggle.setToggleState (persistedOverdriveEnabled, juce::dontSendNotification);
    stylePowerToggle (overdriveToggle, juce::Colour (0xffff9a1f));
    overdriveToggle.onClick = [this, stylePowerToggle]
    {
        const auto v = overdriveToggle.getToggleState();
        overdriveEnabled.store (v, std::memory_order_relaxed);
        if (overdriveProcessor != nullptr)
            overdriveProcessor->setEnabled (v);

        settingsFile.setValue (kKeyOverdriveEnabled, v);
        markSettingsDirty();
        stylePowerToggle (overdriveToggle, juce::Colour (0xffff9a1f));
    };
    addAndMakeVisible (overdriveToggle);

    retryAudioButton.onClick = [this]
    {
        initialiseAudioWithFallback (nullptr);
        rebuildDeviceSelector();
        updateAudioDeviceStateInSettings();
        saveSettingsIfNeeded (true);
        resized();
    };
    addAndMakeVisible (retryAudioButton);
    retryAudioButton.setVisible (false);

    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);

    // Sprint 0: input -> output monitoring + device selection UI
    initialiseAudioWithFallback (persistedAudioState.get());
    deviceManager.addChangeListener (this);
    rebuildDeviceSelector();

    updateAudioDeviceStateInSettings();
    saveSettingsIfNeeded (true);
    retryAudioButton.setVisible (audioInitError.isNotEmpty());

    deviceGroup.toBack();
    monitorGroup.toBack();
    dspChainGroup.toBack();
    cleanBoostGroup.toBack();
    overdriveGroup.toBack();

    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();

    deviceManager.removeChangeListener (this);
    deviceManager.removeAudioCallback (this);

    updateAudioDeviceStateInSettings();
    saveSettingsIfNeeded (true);

    setLookAndFeel (nullptr);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e1e));
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds().reduced (12);

    auto header = bounds.removeFromTop (48);
    auto titleArea = header.removeFromLeft (220);
    titleLabel.setBounds (titleArea.removeFromTop (28));
    versionLabel.setBounds (titleArea);
    retryAudioButton.setBounds (header.removeFromRight (120));
    deviceStatusLabel.setBounds (header);

    auto content = bounds;
    auto topRow = content.removeFromTop ((int) (content.getHeight() * 0.6f));
    auto left = topRow.removeFromLeft ((int) (topRow.getWidth() * 0.65f));
    auto right = topRow;

    deviceGroup.setBounds (left);
    monitorGroup.setBounds (right);
    dspChainGroup.setBounds (content);

    {
        auto deviceArea = deviceGroup.getBounds().reduced (12);
        deviceArea.removeFromTop (22);

        if (deviceSelector != nullptr)
            deviceSelector->setBounds (deviceArea);
    }

    {
        auto monitorArea = monitorGroup.getBounds().reduced (12);
        monitorArea.removeFromTop (22);

        monitorNoteLabel.setBounds (monitorArea.removeFromTop (20));
        monitorArea.removeFromTop (6);

        auto controls = monitorArea.removeFromTop (26);
        monitorEnabledToggle.setBounds (controls.removeFromLeft (110));
        muteToggle.setBounds (controls.removeFromLeft (70));
        routingModeCombo.setBounds (controls.reduced (0, 2));

        monitorArea.removeFromTop (8);
        monitorGainSlider.setBounds (monitorArea.removeFromTop (26));
        monitorArea.removeFromTop (8);

        auto inRow = monitorArea.removeFromTop (26);
        inputMeter.setBounds (inRow.removeFromRight (140).reduced (0, 4));
        inputLevelLabel.setBounds (inRow);

        monitorArea.removeFromTop (6);

        auto outRow = monitorArea.removeFromTop (26);
        outputMeter.setBounds (outRow.removeFromRight (140).reduced (0, 4));
        outputLevelLabel.setBounds (outRow);
    }

    {
        auto chainArea = dspChainGroup.getBounds().reduced (12);
        chainArea.removeFromTop (22);

        auto chainHeader = chainArea.removeFromTop (26);
        globalBypassToggle.setBounds (chainHeader.removeFromRight (160).reduced (0, 2));
        dspChainNoteLabel.setBounds (chainHeader);

        chainArea.removeFromTop (8);

        constexpr int kCardWidth = 220;
        constexpr int kCardGap = 12;

        auto cleanArea = chainArea.removeFromLeft (kCardWidth);
        cleanBoostGroup.setBounds (cleanArea);

        chainArea.removeFromLeft (kCardGap);

        auto overdriveArea = chainArea.removeFromLeft (kCardWidth);
        overdriveGroup.setBounds (overdriveArea);

        {
            auto cardContent = cleanBoostGroup.getBounds().reduced (12);
            cardContent.removeFromTop (22);

            cleanBoostGainLabel.setBounds (cardContent.removeFromTop (18));
            cardContent.removeFromTop (6);

            auto knobArea = cardContent.removeFromTop (110);
            cleanBoostGainSlider.setBounds (knobArea);

            cardContent.removeFromTop (6);
            cleanBoostToggle.setBounds (cardContent.removeFromTop (24));
        }

        {
            auto cardContent = overdriveGroup.getBounds().reduced (12);
            cardContent.removeFromTop (22);

            auto labelsRow = cardContent.removeFromTop (18);
            overdriveDriveLabel.setBounds (labelsRow.removeFromLeft (labelsRow.getWidth() / 2));
            overdriveLevelLabel.setBounds (labelsRow);

            cardContent.removeFromTop (6);

            auto knobsRow = cardContent.removeFromTop (110);
            auto leftKnob = knobsRow.removeFromLeft (knobsRow.getWidth() / 2);
            overdriveDriveSlider.setBounds (leftKnob);
            overdriveLevelSlider.setBounds (knobsRow);

            cardContent.removeFromTop (6);
            overdriveToggle.setBounds (cardContent.removeFromTop (24));
        }

    }
}

void MainComponent::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          [[maybe_unused]] const juce::AudioIODeviceCallbackContext& context)
{
    if (numSamples <= 0)
        return;

    juce::ScopedNoDenormals noDenormals;

    const auto monitorOn = monitorEnabled.load (std::memory_order_relaxed);
    const auto isMuted = muted.load (std::memory_order_relaxed);
    const auto doMonitor = monitorOn && ! isMuted;

    const auto mode = routingMode.load (std::memory_order_relaxed);
    const auto gain = monitorGainLinear.load (std::memory_order_relaxed);

    if (! doMonitor)
    {
        for (int ch = 0; ch < numOutputChannels; ++ch)
            if (auto* out = outputChannelData[ch])
                juce::FloatVectorOperations::clear (out, numSamples);
    }
    else if (mode == 2) // Mono -> Stereo
    {
        const auto* monoIn = (numInputChannels > 0) ? inputChannelData[0] : nullptr;

        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            if (auto* out = outputChannelData[ch])
            {
                if (monoIn != nullptr)
                {
                    juce::FloatVectorOperations::copy (out, monoIn, numSamples);
                    if (gain != 1.0f)
                        juce::FloatVectorOperations::multiply (out, gain, numSamples);
                }
                else
                {
                    juce::FloatVectorOperations::clear (out, numSamples);
                }
            }
        }
    }
    else if (mode == 3 || mode == 4) // Stereo -> Mono
    {
        const auto* in0 = (numInputChannels > 0) ? inputChannelData[0] : nullptr;
        const auto* in1 = (numInputChannels > 1) ? inputChannelData[1] : nullptr;

        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            if (auto* out = outputChannelData[ch])
            {
                const auto shouldWrite = (mode == 3) || (ch == 0);

                if (! shouldWrite || in0 == nullptr)
                {
                    juce::FloatVectorOperations::clear (out, numSamples);
                    continue;
                }

                if (in1 == nullptr)
                {
                    juce::FloatVectorOperations::copy (out, in0, numSamples);
                    if (gain != 1.0f)
                        juce::FloatVectorOperations::multiply (out, gain, numSamples);
                    continue;
                }

                for (int i = 0; i < numSamples; ++i)
                    out[i] = 0.5f * (in0[i] + in1[i]) * gain;
            }
        }
    }
    else // Match channels
    {
        const auto channelsToCopy = juce::jmin (numInputChannels, numOutputChannels);

        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            if (auto* out = outputChannelData[ch])
            {
                if (ch < channelsToCopy && inputChannelData[ch] != nullptr)
                {
                    juce::FloatVectorOperations::copy (out, inputChannelData[ch], numSamples);
                    if (gain != 1.0f)
                        juce::FloatVectorOperations::multiply (out, gain, numSamples);
                }
                else
                {
                    juce::FloatVectorOperations::clear (out, numSamples);
                }
            }
        }
    }

    if (numOutputChannels > 0)
    {
        bool needsScratch = false;

        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            if (outputChannelData[ch] == nullptr)
            {
                needsScratch = true;
                break;
            }
        }

        if (needsScratch)
        {
            if (engineBuffer.getNumChannels() < numOutputChannels || engineBuffer.getNumSamples() < numSamples)
                engineBuffer.setSize (numOutputChannels, numSamples, false, false, true);

            for (int ch = 0; ch < numOutputChannels; ++ch)
            {
                if (auto* out = outputChannelData[ch])
                    engineBuffer.copyFrom (ch, 0, out, numSamples);
                else
                    engineBuffer.clear (ch, 0, numSamples);
            }

            audioEngine.processBlock (engineBuffer);

            for (int ch = 0; ch < numOutputChannels; ++ch)
            {
                if (auto* out = outputChannelData[ch])
                    juce::FloatVectorOperations::copy (out, engineBuffer.getReadPointer (ch), numSamples);
            }
        }
        else
        {
            juce::AudioBuffer<float> outputBuffer (const_cast<float**> (outputChannelData),
                                                   numOutputChannels,
                                                   numSamples);
            audioEngine.processBlock (outputBuffer);
        }
    }

    // Input metering (RMS + peak + clip detection) across all available input channels.
    double sumSquares = 0.0;
    int count = 0;
    float peak = 0.0f;
    bool clipped = false;

    for (int ch = 0; ch < numInputChannels; ++ch)
    {
        if (const auto* in = inputChannelData[ch])
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const auto s = in[i];
                sumSquares += (double) s * (double) s;

                const auto a = std::abs (s);
                peak = juce::jmax (peak, a);
                clipped = clipped || (a >= 0.999f);
            }
            count += numSamples;
        }
    }

    const auto rms = (count > 0) ? (float) std::sqrt (sumSquares / (double) count) : 0.0f;
    inputRms.store (rms, std::memory_order_relaxed);
    inputPeak.store (peak, std::memory_order_relaxed);
    inputClipped.store (clipped, std::memory_order_relaxed);

    // Output metering (pre-mute): compute from routing + gain, regardless of Monitor/Mute state.
    double outSumSquares = 0.0;
    int outCount = 0;
    float outPeak = 0.0f;
    bool outClipped = false;

    if (mode == 2) // Mono -> Stereo
    {
        const auto* monoIn = (numInputChannels > 0) ? inputChannelData[0] : nullptr;
        if (monoIn != nullptr)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const auto s = monoIn[i] * gain;
                outSumSquares += (double) s * (double) s;

                const auto a = std::abs (s);
                outPeak = juce::jmax (outPeak, a);
                outClipped = outClipped || (a >= 0.999f);
            }
            outCount = numSamples;
        }
    }
    else if (mode == 3 || mode == 4) // Stereo -> Mono
    {
        const auto* in0 = (numInputChannels > 0) ? inputChannelData[0] : nullptr;
        const auto* in1 = (numInputChannels > 1) ? inputChannelData[1] : nullptr;

        if (in0 != nullptr)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const auto mono = (in1 != nullptr) ? (0.5f * (in0[i] + in1[i])) : in0[i];
                const auto s = mono * gain;

                outSumSquares += (double) s * (double) s;

                const auto a = std::abs (s);
                outPeak = juce::jmax (outPeak, a);
                outClipped = outClipped || (a >= 0.999f);
            }
            outCount = numSamples;
        }
    }
    else // Match channels
    {
        const auto channelsToCopy = juce::jmin (numInputChannels, numOutputChannels);

        for (int ch = 0; ch < channelsToCopy; ++ch)
        {
            if (const auto* in = inputChannelData[ch])
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    const auto s = in[i] * gain;
                    outSumSquares += (double) s * (double) s;

                    const auto a = std::abs (s);
                    outPeak = juce::jmax (outPeak, a);
                    outClipped = outClipped || (a >= 0.999f);
                }
                outCount += numSamples;
            }
        }
    }

    const auto outRms = (outCount > 0) ? (float) std::sqrt (outSumSquares / (double) outCount) : 0.0f;
    outputRms.store (outRms, std::memory_order_relaxed);
    outputPeak.store (outPeak, std::memory_order_relaxed);
    outputClipped.store (outClipped, std::memory_order_relaxed);
}

void MainComponent::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    inputRms.store (0.0f, std::memory_order_relaxed);
    inputPeak.store (0.0f, std::memory_order_relaxed);
    inputClipped.store (false, std::memory_order_relaxed);

    outputRms.store (0.0f, std::memory_order_relaxed);
    outputPeak.store (0.0f, std::memory_order_relaxed);
    outputClipped.store (false, std::memory_order_relaxed);

    if (device != nullptr)
    {
        const auto numOutputChannels = device->getActiveOutputChannels().countNumberOfSetBits();
        audioEngine.prepareToPlay (device->getCurrentSampleRate(),
                                   device->getCurrentBufferSizeSamples(),
                                   numOutputChannels);
        engineBuffer.setSize (numOutputChannels,
                              device->getCurrentBufferSizeSamples(),
                              false,
                              false,
                              true);
    }
}

void MainComponent::audioDeviceStopped()
{
    inputRms.store (0.0f, std::memory_order_relaxed);
    inputPeak.store (0.0f, std::memory_order_relaxed);
    inputClipped.store (false, std::memory_order_relaxed);

    outputRms.store (0.0f, std::memory_order_relaxed);
    outputPeak.store (0.0f, std::memory_order_relaxed);
    outputClipped.store (false, std::memory_order_relaxed);

    audioEngine.reset();
}

void MainComponent::audioDeviceError (const juce::String& errorMessage)
{
    juce::Component::SafePointer<MainComponent> safeThis (this);
    auto msg = errorMessage;

    juce::MessageManager::callAsync ([safeThis, msg]
    {
        if (safeThis == nullptr)
            return;

        safeThis->audioInitError = "Audio device error: " + msg;
        safeThis->retryAudioButton.setVisible (true);
    });
}

void MainComponent::timerCallback()
{
    juce::String status;

    if (audioInitError.isNotEmpty())
    {
        status = "Audio init failed: " + audioInitError;
    }
    else if (auto* dev = deviceManager.getCurrentAudioDevice())
    {
        status = juce::String ("Device: ") + dev->getName()
            + " | SR: " + juce::String (dev->getCurrentSampleRate(), 0)
            + " Hz | Buffer: " + juce::String (dev->getCurrentBufferSizeSamples()) + " samples"
            + " | CPU: " + juce::String (deviceManager.getCpuUsage() * 100.0, 1) + "%";
    }
    else
    {
        status = "Device: (none)";
    }

    if (audioInitNote.isNotEmpty())
        status += " | " + audioInitNote;

    deviceStatusLabel.setText (status, juce::dontSendNotification);
    retryAudioButton.setVisible (audioInitError.isNotEmpty());

    const auto monOn = monitorEnabled.load (std::memory_order_relaxed);
    const auto isMuted = muted.load (std::memory_order_relaxed);
    const auto mode = routingMode.load (std::memory_order_relaxed);
    const auto gainDb = monitorGainDb.load (std::memory_order_relaxed);

    {
        juce::String routing;
        if (mode == 2) routing = "Mono -> Stereo";
        else if (mode == 3) routing = "Stereo -> Mono (all outs)";
        else if (mode == 4) routing = "Stereo -> Mono (L only)";
        else routing = "Match channels";

        auto note = juce::String (monOn ? "Monitor ON" : "Monitor OFF");
        if (isMuted)
            note += " | MUTE";
        note += " | " + routing;
        note += " | Gain " + juce::String (gainDb, 1) + " dB";

        monitorNoteLabel.setText (note, juce::dontSendNotification);
    }

    {
        const auto bypassed = globalBypass.load (std::memory_order_relaxed);
        const auto boostOn = cleanBoostEnabled.load (std::memory_order_relaxed);
        const auto boostDb = cleanBoostGainDb.load (std::memory_order_relaxed);

        const auto odOn = overdriveEnabled.load (std::memory_order_relaxed);
        const auto odDrive = overdriveDrivePct.load (std::memory_order_relaxed);
        const auto odLevel = overdriveLevelPct.load (std::memory_order_relaxed);

        juce::String chain = bypassed ? "Bypassed"
                                      : "Clean Boost " + juce::String (boostOn ? "ON" : "OFF")
                                            + " | Gain " + juce::String (boostDb, 1) + " dB"
                                            + " -> Overdrive " + juce::String (odOn ? "ON" : "OFF")
                                            + " | Drive " + juce::String (odDrive, 0) + "%"
                                            + " | Level " + juce::String (odLevel, 0) + "%";

        dspChainNoteLabel.setText ("DSP Chain: " + chain, juce::dontSendNotification);
    }

    const auto rms = inputRms.load (std::memory_order_relaxed);
    const auto peak = inputPeak.load (std::memory_order_relaxed);
    const auto clipped = inputClipped.load (std::memory_order_relaxed);

    const auto rmsDb = rmsToDb (rms);
    const auto peakDb = rmsToDb (peak);

    const auto now = juce::Time::getMillisecondCounter();
    if (peakHoldLastUpdateMs == 0)
        peakHoldLastUpdateMs = now;

    if (peakDb >= peakHoldDb)
    {
        peakHoldDb = peakDb;
        peakHoldLastUpdateMs = now;
    }
    else if (now - peakHoldLastUpdateMs > 500)
    {
        peakHoldDb = juce::jmax (peakDb, peakHoldDb - 1.0f);
    }

    auto levelText = juce::String ("Input: RMS ") + juce::String (rmsDb, 1)
        + " dB | Peak " + juce::String (peakHoldDb, 1) + " dB";

    if (clipped)
        levelText += " | CLIP";

    inputLevelLabel.setText (levelText, juce::dontSendNotification);
    inputMeter.setLevelsDb (rmsDb, peakHoldDb, clipped);

    const auto outRms = outputRms.load (std::memory_order_relaxed);
    const auto outPeak = outputPeak.load (std::memory_order_relaxed);
    const auto outClipped = outputClipped.load (std::memory_order_relaxed);

    const auto outRmsDb = rmsToDb (outRms);
    const auto outPeakDb = rmsToDb (outPeak);

    if (outputPeakHoldLastUpdateMs == 0)
        outputPeakHoldLastUpdateMs = now;

    if (outPeakDb >= outputPeakHoldDb)
    {
        outputPeakHoldDb = outPeakDb;
        outputPeakHoldLastUpdateMs = now;
    }
    else if (now - outputPeakHoldLastUpdateMs > 500)
    {
        outputPeakHoldDb = juce::jmax (outPeakDb, outputPeakHoldDb - 1.0f);
    }

    const auto outPrefix = (! monOn || isMuted) ? "Output (pre): RMS " : "Output: RMS ";
    auto outText = juce::String (outPrefix) + juce::String (outRmsDb, 1)
        + " dB | Peak " + juce::String (outputPeakHoldDb, 1) + " dB";

    if (outClipped)
        outText += " | CLIP";

    outputLevelLabel.setText (outText, juce::dontSendNotification);
    outputMeter.setLevelsDb (outRmsDb, outputPeakHoldDb, outClipped);

    // If we didn't manage to persist audio device state during startup, retry periodically once a device is available.
    if (settingsFile.getValue (kKeyAudioDeviceStateXml).isEmpty())
    {
        if (lastDeviceStatePersistTryMs == 0 || now - lastDeviceStatePersistTryMs > 2000)
        {
            lastDeviceStatePersistTryMs = now;
            updateAudioDeviceStateInSettings();
        }
    }

    saveSettingsIfNeeded (false);
}
