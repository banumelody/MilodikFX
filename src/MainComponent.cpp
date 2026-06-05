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

void MainComponent::EffectCard::setTitle (juce::String newTitle)
{
    title = std::move (newTitle);
    repaint();
}

void MainComponent::EffectCard::setAccentColour (juce::Colour newAccent)
{
    accent = newAccent;
    repaint();
}

void MainComponent::EffectCard::setEnabledState (bool isEnabled)
{
    enabledState = isEnabled;
    repaint();
}

juce::Rectangle<int> MainComponent::EffectCard::getContentBounds() const
{
    auto b = getLocalBounds().reduced (14);
    b.removeFromTop (38);
    b.removeFromTop (10);
    b.removeFromBottom (10);
    return b;
}

void MainComponent::EffectCard::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    const auto radius = 10.0f;

    // Body
    {
        juce::ColourGradient bodyGrad (juce::Colour (0xff232629), b.getCentreX(), b.getY(),
                                      juce::Colour (0xff121416), b.getCentreX(), b.getBottom(), false);
        g.setGradientFill (bodyGrad);
        g.fillRoundedRectangle (b, radius);

        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.drawRoundedRectangle (b, radius, 1.5f);
    }

    // Header
    {
        auto header = b;
        header.setHeight (38.0f);

        const auto headerAccent = enabledState ? accent.withAlpha (0.55f)
                                               : accent.withAlpha (0.14f);

        juce::ColourGradient headerGrad (headerAccent, header.getX(), header.getY(),
                                        juce::Colour (0xff0f1113), header.getX(), header.getBottom(), false);
        g.setGradientFill (headerGrad);
        g.fillRoundedRectangle (header, radius);

        // Square off the bottom of the header for a nicer card look
        g.setColour (juce::Colour (0xff0f1113).withAlpha (0.8f));
        g.fillRect (juce::Rectangle<float> (header.getX(), header.getBottom() - radius, header.getWidth(), radius));

        // LED
        const auto led = enabledState ? accent.withAlpha (0.95f)
                                      : juce::Colours::black.withAlpha (0.35f);
        g.setColour (led);
        g.fillEllipse (juce::Rectangle<float> (header.getX() + 12.0f, header.getCentreY() - 6.0f, 12.0f, 12.0f));
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawEllipse (juce::Rectangle<float> (header.getX() + 12.0f, header.getCentreY() - 6.0f, 12.0f, 12.0f), 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
        g.drawFittedText (title, header.toNearestInt().reduced (24, 0), juce::Justification::centred, 1);
    }
}

void MainComponent::FootswitchButton::setAccentColour (juce::Colour newAccent)
{
    accent = newAccent;
    repaint();
}

void MainComponent::FootswitchButton::paintButton (juce::Graphics& g,
                                                   bool shouldDrawButtonAsHighlighted,
                                                   bool shouldDrawButtonAsDown)
{
    auto b = getLocalBounds().toFloat().reduced (2.0f);

    const auto radius = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto cx = b.getCentreX();
    const auto cy = b.getCentreY();

    const auto circle = juce::Rectangle<float> (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    const auto toggleOn = getToggleState();

    auto base1 = juce::Colour (0xff2b2e31);
    auto base2 = juce::Colour (0xff0d0f11);

    if (shouldDrawButtonAsDown)
        base1 = base1.brighter (0.08f);

    if (shouldDrawButtonAsHighlighted)
        base1 = base1.brighter (0.05f);

    juce::ColourGradient grad (base1, cx, cy - radius, base2, cx, cy + radius, false);
    g.setGradientFill (grad);
    g.fillEllipse (circle);

    // Outer ring
    g.setColour (juce::Colours::black.withAlpha (0.75f));
    g.drawEllipse (circle, 2.0f);

    // Inner ring
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawEllipse (circle.reduced (4.0f), 1.0f);

    // Accent glow when ON
    if (toggleOn)
    {
        g.setColour (accent.withAlpha (0.55f));
        g.drawEllipse (circle.reduced (1.5f), 3.0f);
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
        deviceSelectorViewport.setViewedComponent (nullptr, false);

    deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent> (
        deviceManager,
        0, 2,   // min/max inputs
        1, 2,   // min/max outputs
        false,  // show MIDI input
        false,  // show MIDI output
        true,   // show channels
        true    // show advanced
    );

    deviceSelectorViewport.setViewedComponent (deviceSelector.get(), false);
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

milodikfx::preset::PresetState MainComponent::capturePresetState() const
{
    milodikfx::preset::PresetState s;

    s.globalBypass = globalBypass.load (std::memory_order_relaxed);

    s.cleanBoostEnabled = cleanBoostEnabled.load (std::memory_order_relaxed);
    s.cleanBoostGainDb = cleanBoostGainDb.load (std::memory_order_relaxed);

    s.overdriveEnabled = overdriveEnabled.load (std::memory_order_relaxed);
    s.overdriveDrivePct = overdriveDrivePct.load (std::memory_order_relaxed);
    s.overdriveLevelPct = overdriveLevelPct.load (std::memory_order_relaxed);

    s.eqEnabled = eqEnabled.load (std::memory_order_relaxed);
    s.eqBassDb = eqBassDb.load (std::memory_order_relaxed);
    s.eqMidDb = eqMidDb.load (std::memory_order_relaxed);
    s.eqTrebleDb = eqTrebleDb.load (std::memory_order_relaxed);

    return s;
}

void MainComponent::applyPresetState (const milodikfx::preset::PresetState& p)
{
    const auto cleanAccent = juce::Colour (0xff6ee04a);
    const auto odAccent = juce::Colour (0xffff9a1f);
    const auto eqAccent = juce::Colour (0xff3aa3ff);

    auto setEffectStateUi = [] (EffectCard& card, juce::Label& stateLabel, bool isOn, juce::Colour accent)
    {
        card.setEnabledState (isOn);
        stateLabel.setText (isOn ? "ON" : "OFF", juce::dontSendNotification);
        stateLabel.setColour (juce::Label::textColourId,
                              isOn ? accent : juce::Colours::white.withAlpha (0.5f));
    };

    globalBypass.store (p.globalBypass, std::memory_order_relaxed);
    audioEngine.setBypassed (p.globalBypass);
    globalBypassToggle.setToggleState (p.globalBypass, juce::dontSendNotification);
    settingsFile.setValue (kKeyGlobalBypass, p.globalBypass);

    cleanBoostGainSlider.setValue (p.cleanBoostGainDb, juce::dontSendNotification);
    cleanBoostGainDb.store (p.cleanBoostGainDb, std::memory_order_relaxed);
    if (cleanBoostProcessor != nullptr)
        cleanBoostProcessor->setGainDb (p.cleanBoostGainDb);
    settingsFile.setValue (kKeyCleanBoostGainDb, (double) p.cleanBoostGainDb);

    cleanBoostToggle.setToggleState (p.cleanBoostEnabled, juce::dontSendNotification);
    cleanBoostEnabled.store (p.cleanBoostEnabled, std::memory_order_relaxed);
    if (cleanBoostProcessor != nullptr)
        cleanBoostProcessor->setEnabled (p.cleanBoostEnabled);
    settingsFile.setValue (kKeyCleanBoostEnabled, p.cleanBoostEnabled);
    setEffectStateUi (cleanBoostCard, cleanBoostStateLabel, p.cleanBoostEnabled, cleanAccent);

    overdriveDriveSlider.setValue (p.overdriveDrivePct, juce::dontSendNotification);
    overdriveDrivePct.store (p.overdriveDrivePct, std::memory_order_relaxed);
    if (overdriveProcessor != nullptr)
        overdriveProcessor->setDrivePercent (p.overdriveDrivePct);
    settingsFile.setValue (kKeyOverdriveDrivePct, (double) p.overdriveDrivePct);

    overdriveLevelSlider.setValue (p.overdriveLevelPct, juce::dontSendNotification);
    overdriveLevelPct.store (p.overdriveLevelPct, std::memory_order_relaxed);
    if (overdriveProcessor != nullptr)
        overdriveProcessor->setLevelPercent (p.overdriveLevelPct);
    settingsFile.setValue (kKeyOverdriveLevelPct, (double) p.overdriveLevelPct);

    overdriveToggle.setToggleState (p.overdriveEnabled, juce::dontSendNotification);
    overdriveEnabled.store (p.overdriveEnabled, std::memory_order_relaxed);
    if (overdriveProcessor != nullptr)
        overdriveProcessor->setEnabled (p.overdriveEnabled);
    settingsFile.setValue (kKeyOverdriveEnabled, p.overdriveEnabled);
    setEffectStateUi (overdriveCard, overdriveStateLabel, p.overdriveEnabled, odAccent);

    eqBassSlider.setValue (p.eqBassDb, juce::dontSendNotification);
    eqBassDb.store (p.eqBassDb, std::memory_order_relaxed);
    if (eqProcessor != nullptr)
        eqProcessor->setBassDb (p.eqBassDb);
    settingsFile.setValue (kKeyEqBassDb, (double) p.eqBassDb);

    eqMidSlider.setValue (p.eqMidDb, juce::dontSendNotification);
    eqMidDb.store (p.eqMidDb, std::memory_order_relaxed);
    if (eqProcessor != nullptr)
        eqProcessor->setMidDb (p.eqMidDb);
    settingsFile.setValue (kKeyEqMidDb, (double) p.eqMidDb);

    eqTrebleSlider.setValue (p.eqTrebleDb, juce::dontSendNotification);
    eqTrebleDb.store (p.eqTrebleDb, std::memory_order_relaxed);
    if (eqProcessor != nullptr)
        eqProcessor->setTrebleDb (p.eqTrebleDb);
    settingsFile.setValue (kKeyEqTrebleDb, (double) p.eqTrebleDb);

    eqToggle.setToggleState (p.eqEnabled, juce::dontSendNotification);
    eqEnabled.store (p.eqEnabled, std::memory_order_relaxed);
    if (eqProcessor != nullptr)
        eqProcessor->setEnabled (p.eqEnabled);
    settingsFile.setValue (kKeyEqEnabled, p.eqEnabled);
    setEffectStateUi (eqCard, eqStateLabel, p.eqEnabled, eqAccent);

    markSettingsDirty();
    saveSettingsIfNeeded (true);
}

void MainComponent::refreshPresetList (const juce::String& selectPresetName)
{
    const auto names = presetManager.listPresets();

    presetCombo.clear();

    int id = 1;
    for (const auto& n : names)
        presetCombo.addItem (n, id++);

    juce::String toSelect = selectPresetName;
    if (toSelect.isEmpty() && names.size() > 0)
        toSelect = names[0];

    int selectedId = 0;
    for (int i = 0; i < presetCombo.getNumItems(); ++i)
        if (presetCombo.getItemText (i) == toSelect)
            selectedId = presetCombo.getItemId (i);

    if (selectedId == 0 && presetCombo.getNumItems() > 0)
        selectedId = presetCombo.getItemId (0);

    if (selectedId != 0)
        presetCombo.setSelectedId (selectedId, juce::dontSendNotification);

    const auto hasSelection = presetCombo.getNumItems() > 0 && presetCombo.getSelectedId() != 0;
    presetLoadButton.setEnabled (hasSelection);

    const auto selName = presetCombo.getText();
    presetDeleteButton.setEnabled (hasSelection && selName != "Default Clean");
}

MainComponent::MainComponent (juce::PropertiesFile& settings)
    : settingsFile (settings),
      presetManager (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                         .getChildFile ("MilodikFX")
                         .getChildFile ("Presets"))
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

    double persistedEqBassDb = 0.0;
    double persistedEqMidDb = 0.0;
    double persistedEqTrebleDb = 0.0;
    bool persistedEqEnabled = true;

    juce::String persistedAudioXml;
    juce::String persistedPresetName;

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

    persistedEqBassDb = settingsFile.getDoubleValue (kKeyEqBassDb, 0.0);
    persistedEqMidDb = settingsFile.getDoubleValue (kKeyEqMidDb, 0.0);
    persistedEqTrebleDb = settingsFile.getDoubleValue (kKeyEqTrebleDb, 0.0);
    persistedEqEnabled = settingsFile.getBoolValue (kKeyEqEnabled, true);

    persistedAudioXml = settingsFile.getValue (kKeyAudioDeviceStateXml);
    persistedPresetName = settingsFile.getValue (kKeySelectedPresetName, "Default Clean");

    persistedRouting = juce::jlimit (1, 4, persistedRouting);
    persistedGainDb = juce::jlimit (-24.0, 12.0, persistedGainDb);
    persistedCleanBoostGainDb = juce::jlimit (0.0, 24.0, persistedCleanBoostGainDb);

    persistedOverdriveDrivePct = juce::jlimit (0.0, 100.0, persistedOverdriveDrivePct);
    persistedOverdriveLevelPct = juce::jlimit (0.0, 100.0, persistedOverdriveLevelPct);

    persistedEqBassDb = juce::jlimit (-12.0, 12.0, persistedEqBassDb);
    persistedEqMidDb = juce::jlimit (-12.0, 12.0, persistedEqMidDb);
    persistedEqTrebleDb = juce::jlimit (-12.0, 12.0, persistedEqTrebleDb);

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

    eqBassDb.store ((float) persistedEqBassDb, std::memory_order_relaxed);
    eqMidDb.store ((float) persistedEqMidDb, std::memory_order_relaxed);
    eqTrebleDb.store ((float) persistedEqTrebleDb, std::memory_order_relaxed);
    eqEnabled.store (persistedEqEnabled, std::memory_order_relaxed);

    if (auto* processor = audioEngine.getChain().addProcessor (std::make_unique<milodikfx::dsp::GainProcessor>()))
        cleanBoostProcessor = dynamic_cast<milodikfx::dsp::GainProcessor*> (processor);

    if (auto* processor = audioEngine.getChain().addProcessor (std::make_unique<milodikfx::dsp::OverdriveProcessor>()))
        overdriveProcessor = dynamic_cast<milodikfx::dsp::OverdriveProcessor*> (processor);

    if (auto* processor = audioEngine.getChain().addProcessor (std::make_unique<milodikfx::dsp::EQProcessor>()))
        eqProcessor = dynamic_cast<milodikfx::dsp::EQProcessor*> (processor);

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

    if (eqProcessor != nullptr)
    {
        eqProcessor->setBassDb ((float) persistedEqBassDb);
        eqProcessor->setMidDb ((float) persistedEqMidDb);
        eqProcessor->setTrebleDb ((float) persistedEqTrebleDb);
        eqProcessor->setEnabled (persistedEqEnabled);
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
    settingsFile.setValue (kKeyEqBassDb, persistedEqBassDb);
    settingsFile.setValue (kKeyEqMidDb, persistedEqMidDb);
    settingsFile.setValue (kKeyEqTrebleDb, persistedEqTrebleDb);
    settingsFile.setValue (kKeyEqEnabled, persistedEqEnabled);
    settingsFile.setValue (kKeySelectedPresetName, persistedPresetName);
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

    presetLabel.setText ("Preset:", juce::dontSendNotification);
    presetLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (presetLabel);

    presetCombo.setTextWhenNothingSelected ("(No presets)");
    presetCombo.onChange = [this]
    {
        const auto safe = milodikfx::preset::PresetManager::sanitisePresetName (presetCombo.getText());
        if (safe.isNotEmpty())
        {
            settingsFile.setValue (kKeySelectedPresetName, juce::var (safe));
            markSettingsDirty();
        }

        const auto hasSelection = presetCombo.getNumItems() > 0 && presetCombo.getSelectedId() != 0;
        presetLoadButton.setEnabled (hasSelection);
        presetDeleteButton.setEnabled (hasSelection && presetCombo.getText() != "Default Clean");
    };
    addAndMakeVisible (presetCombo);

    presetSaveButton.onClick = [this]
    {
        const auto initial = presetCombo.getText().isNotEmpty() ? presetCombo.getText() : juce::String ("My Preset");
        juce::Component::SafePointer<MainComponent> self (this);

        auto* w = new juce::AlertWindow ("Save Preset", "Preset name:", juce::AlertWindow::QuestionIcon, this);
        w->addTextEditor ("name", initial, "Name");
        w->addButton ("Save", 1);
        w->addButton ("Cancel", 0);

        w->enterModalState (true,
                            juce::ModalCallbackFunction::create ([self, w] (int result)
                            {
                                if (self == nullptr)
                                    return;

                                if (result != 1)
                                    return;

                                auto name = w->getTextEditorContents ("name");
                                name = milodikfx::preset::PresetManager::sanitisePresetName (name);
                                if (name.isEmpty())
                                    return;

                                const auto ok = self->presetManager.savePreset (name, self->capturePresetState());
                                if (! ok)
                                {
                                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                                            "Save Preset",
                                                                            "Failed to save preset.");
                                    return;
                                }

                                self->refreshPresetList (name);
                                self->settingsFile.setValue (kKeySelectedPresetName, juce::var (name));
                                self->markSettingsDirty();
                                self->saveSettingsIfNeeded (true);
                            }),
                            true);
    };
    addAndMakeVisible (presetSaveButton);

    presetLoadButton.onClick = [this]
    {
        const auto name = presetCombo.getText();
        milodikfx::preset::PresetState loaded;

        if (! presetManager.loadPreset (name, loaded))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                    "Load Preset",
                                                    "Failed to load preset.");
            return;
        }

        applyPresetState (loaded);
        settingsFile.setValue (kKeySelectedPresetName, juce::var (name));
        markSettingsDirty();
        saveSettingsIfNeeded (true);
    };
    addAndMakeVisible (presetLoadButton);

    presetDeleteButton.onClick = [this]
    {
        const auto name = presetCombo.getText();
        if (name == "Default Clean")
            return;

        juce::Component::SafePointer<MainComponent> self (this);

        juce::AlertWindow::showOkCancelBox (juce::AlertWindow::WarningIcon,
                                            "Delete Preset",
                                            "Delete preset '" + name + "'?",
                                            "Delete",
                                            "Cancel",
                                            this,
                                            juce::ModalCallbackFunction::create ([self, name] (int result)
                                            {
                                                if (self == nullptr)
                                                    return;

                                                if (result != 1)
                                                    return;

                                                self->presetManager.deletePreset (name);

                                                milodikfx::preset::PresetState defaultPreset;
                                                self->presetManager.ensurePresetExists ("Default Clean", defaultPreset);

                                                self->refreshPresetList ("Default Clean");

                                                milodikfx::preset::PresetState loaded;
                                                if (self->presetManager.loadPreset ("Default Clean", loaded))
                                                    self->applyPresetState (loaded);

                                                self->settingsFile.setValue (kKeySelectedPresetName, juce::var ("Default Clean"));
                                                self->markSettingsDirty();
                                                self->saveSettingsIfNeeded (true);
                                            }));
    };
    addAndMakeVisible (presetDeleteButton);

    deviceGroup.setText ("Audio Device");
    addAndMakeVisible (deviceGroup);

    deviceSelectorViewport.setScrollBarsShown (true, false);
    deviceSelectorViewport.setScrollBarThickness (10);
    deviceSelectorViewport.setWantsKeyboardFocus (false);
    addAndMakeVisible (deviceSelectorViewport);

    monitorGroup.setText ("Monitoring");
    addAndMakeVisible (monitorGroup);

    dspChainGroup.setText ("DSP Chain");
    addAndMakeVisible (dspChainGroup);

    cleanBoostCard.setTitle ("CLEAN BOOST");
    cleanBoostCard.setAccentColour (juce::Colour (0xff6ee04a));
    addAndMakeVisible (cleanBoostCard);

    overdriveCard.setTitle ("OVERDRIVE");
    overdriveCard.setAccentColour (juce::Colour (0xffff9a1f));
    addAndMakeVisible (overdriveCard);

    eqCard.setTitle ("3 BAND EQ");
    eqCard.setAccentColour (juce::Colour (0xff3aa3ff));
    addAndMakeVisible (eqCard);

    cleanBoostCard.setEnabledState (persistedCleanBoostEnabled);
    overdriveCard.setEnabledState (persistedOverdriveEnabled);
    eqCard.setEnabledState (persistedEqEnabled);

    monitorNoteLabel.setText ("Passthrough: input -> output", juce::dontSendNotification);
    monitorNoteLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (monitorNoteLabel);

    inputLevelLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (inputLevelLabel);

    outputLevelLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (outputLevelLabel);

    dspChainNoteLabel.setText ("DSP Chain: Clean Boost -> Overdrive -> EQ", juce::dontSendNotification);
    dspChainNoteLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (dspChainNoteLabel);

    cleanBoostGainLabel.setText ("Gain", juce::dontSendNotification);
    cleanBoostGainLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (cleanBoostGainLabel);

    cleanBoostStateLabel.setJustificationType (juce::Justification::centred);
    cleanBoostStateLabel.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    addAndMakeVisible (cleanBoostStateLabel);

    overdriveDriveLabel.setText ("Drive", juce::dontSendNotification);
    overdriveDriveLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (overdriveDriveLabel);

    overdriveLevelLabel.setText ("Level", juce::dontSendNotification);
    overdriveLevelLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (overdriveLevelLabel);

    overdriveStateLabel.setJustificationType (juce::Justification::centred);
    overdriveStateLabel.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    addAndMakeVisible (overdriveStateLabel);

    eqBassLabel.setText ("Bass", juce::dontSendNotification);
    eqBassLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (eqBassLabel);

    eqMidLabel.setText ("Mid", juce::dontSendNotification);
    eqMidLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (eqMidLabel);

    eqTrebleLabel.setText ("Treble", juce::dontSendNotification);
    eqTrebleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (eqTrebleLabel);

    eqStateLabel.setJustificationType (juce::Justification::centred);
    eqStateLabel.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    addAndMakeVisible (eqStateLabel);

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

    auto setEffectStateUi = [this] (EffectCard& card, juce::Label& stateLabel, bool isOn, juce::Colour accent)
    {
        card.setEnabledState (isOn);

        stateLabel.setText (isOn ? "ON" : "OFF", juce::dontSendNotification);
        stateLabel.setColour (juce::Label::textColourId,
                              isOn ? accent : juce::Colours::white.withAlpha (0.5f));
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

    const auto cleanAccent = juce::Colour (0xff6ee04a);
    cleanBoostToggle.setButtonText ({});
    cleanBoostToggle.setAccentColour (cleanAccent);
    cleanBoostToggle.setToggleState (persistedCleanBoostEnabled, juce::dontSendNotification);
    setEffectStateUi (cleanBoostCard, cleanBoostStateLabel, persistedCleanBoostEnabled, cleanAccent);

    cleanBoostToggle.onClick = [this, cleanAccent, setEffectStateUi]
    {
        const auto v = cleanBoostToggle.getToggleState();
        cleanBoostEnabled.store (v, std::memory_order_relaxed);
        if (cleanBoostProcessor != nullptr)
            cleanBoostProcessor->setEnabled (v);

        settingsFile.setValue (kKeyCleanBoostEnabled, v);
        markSettingsDirty();

        setEffectStateUi (cleanBoostCard, cleanBoostStateLabel, v, cleanAccent);
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

    const auto odAccent = juce::Colour (0xffff9a1f);
    overdriveToggle.setButtonText ({});
    overdriveToggle.setAccentColour (odAccent);
    overdriveToggle.setToggleState (persistedOverdriveEnabled, juce::dontSendNotification);
    setEffectStateUi (overdriveCard, overdriveStateLabel, persistedOverdriveEnabled, odAccent);

    overdriveToggle.onClick = [this, odAccent, setEffectStateUi]
    {
        const auto v = overdriveToggle.getToggleState();
        overdriveEnabled.store (v, std::memory_order_relaxed);
        if (overdriveProcessor != nullptr)
            overdriveProcessor->setEnabled (v);

        settingsFile.setValue (kKeyOverdriveEnabled, v);
        markSettingsDirty();

        setEffectStateUi (overdriveCard, overdriveStateLabel, v, odAccent);
    };
    addAndMakeVisible (overdriveToggle);

    const auto eqAccent = juce::Colour (0xff3aa3ff);

    configureKnob (eqBassSlider, eqAccent);
    eqBassSlider.setTextValueSuffix (" dB");
    eqBassSlider.setRange (-12.0, 12.0, 0.1);
    eqBassSlider.setValue (persistedEqBassDb, juce::dontSendNotification);
    eqBassSlider.onValueChange = [this]
    {
        const auto db = (float) eqBassSlider.getValue();
        eqBassDb.store (db, std::memory_order_relaxed);
        if (eqProcessor != nullptr)
            eqProcessor->setBassDb (db);

        settingsFile.setValue (kKeyEqBassDb, (double) db);
        markSettingsDirty();
    };
    addAndMakeVisible (eqBassSlider);

    configureKnob (eqMidSlider, eqAccent);
    eqMidSlider.setTextValueSuffix (" dB");
    eqMidSlider.setRange (-12.0, 12.0, 0.1);
    eqMidSlider.setValue (persistedEqMidDb, juce::dontSendNotification);
    eqMidSlider.onValueChange = [this]
    {
        const auto db = (float) eqMidSlider.getValue();
        eqMidDb.store (db, std::memory_order_relaxed);
        if (eqProcessor != nullptr)
            eqProcessor->setMidDb (db);

        settingsFile.setValue (kKeyEqMidDb, (double) db);
        markSettingsDirty();
    };
    addAndMakeVisible (eqMidSlider);

    configureKnob (eqTrebleSlider, eqAccent);
    eqTrebleSlider.setTextValueSuffix (" dB");
    eqTrebleSlider.setRange (-12.0, 12.0, 0.1);
    eqTrebleSlider.setValue (persistedEqTrebleDb, juce::dontSendNotification);
    eqTrebleSlider.onValueChange = [this]
    {
        const auto db = (float) eqTrebleSlider.getValue();
        eqTrebleDb.store (db, std::memory_order_relaxed);
        if (eqProcessor != nullptr)
            eqProcessor->setTrebleDb (db);

        settingsFile.setValue (kKeyEqTrebleDb, (double) db);
        markSettingsDirty();
    };
    addAndMakeVisible (eqTrebleSlider);

    eqToggle.setButtonText ({});
    eqToggle.setAccentColour (eqAccent);
    eqToggle.setToggleState (persistedEqEnabled, juce::dontSendNotification);
    setEffectStateUi (eqCard, eqStateLabel, persistedEqEnabled, eqAccent);

    eqToggle.onClick = [this, eqAccent, setEffectStateUi]
    {
        const auto v = eqToggle.getToggleState();
        eqEnabled.store (v, std::memory_order_relaxed);
        if (eqProcessor != nullptr)
            eqProcessor->setEnabled (v);

        settingsFile.setValue (kKeyEqEnabled, v);
        markSettingsDirty();

        setEffectStateUi (eqCard, eqStateLabel, v, eqAccent);
    };
    addAndMakeVisible (eqToggle);

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

    // Sprint 5: presets
    milodikfx::preset::PresetState defaultPreset;
    presetManager.ensurePresetExists ("Default Clean", defaultPreset);
    refreshPresetList (persistedPresetName);

    milodikfx::preset::PresetState startupPreset;
    const auto startupName = presetCombo.getText();
    if (startupName.isNotEmpty() && presetManager.loadPreset (startupName, startupPreset))
        applyPresetState (startupPreset);

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
    auto takeTop = [] (juce::Rectangle<int>& r, int h)
    {
        return r.removeFromTop (juce::jmin (h, r.getHeight()));
    };

    auto takeBottom = [] (juce::Rectangle<int>& r, int h)
    {
        return r.removeFromBottom (juce::jmin (h, r.getHeight()));
    };

    auto takeLeft = [] (juce::Rectangle<int>& r, int w)
    {
        return r.removeFromLeft (juce::jmin (w, r.getWidth()));
    };

    auto takeRight = [] (juce::Rectangle<int>& r, int w)
    {
        return r.removeFromRight (juce::jmin (w, r.getWidth()));
    };

    auto bounds = getLocalBounds().reduced (12);

    auto header = takeTop (bounds, 48);
    auto titleArea = takeLeft (header, 220);
    titleLabel.setBounds (takeTop (titleArea, 28));
    versionLabel.setBounds (titleArea);
    retryAudioButton.setBounds (takeRight (header, 120));
    deviceStatusLabel.setBounds (header);

    auto presetBar = takeTop (bounds, 44);
    {
        auto presetArea = presetBar.reduced (0, 8);

        presetLabel.setBounds (takeLeft (presetArea, juce::jmin (60, presetArea.getWidth())));

        constexpr int kBtnW = 64;
        constexpr int kBtnGap = 8;
        const int desiredButtonsW = kBtnW * 3 + kBtnGap * 2;

        auto buttons = takeRight (presetArea, juce::jmin (desiredButtonsW, presetArea.getWidth()));
        presetCombo.setBounds (presetArea.reduced (0, 2));

        auto saveArea = takeLeft (buttons, juce::jmin (kBtnW, buttons.getWidth()));
        takeLeft (buttons, juce::jmin (kBtnGap, buttons.getWidth()));
        auto loadArea = takeLeft (buttons, juce::jmin (kBtnW, buttons.getWidth()));
        takeLeft (buttons, juce::jmin (kBtnGap, buttons.getWidth()));
        auto deleteArea = buttons;

        presetSaveButton.setBounds (saveArea.reduced (0, 2));
        presetLoadButton.setBounds (loadArea.reduced (0, 2));
        presetDeleteButton.setBounds (deleteArea.reduced (0, 2));
    }

    auto content = bounds;

    const auto desiredTopRowH = (int) (content.getHeight() * 0.6f);
    constexpr int kMinChainH = 260;
    int topRowH = desiredTopRowH;

    if (content.getHeight() - topRowH < kMinChainH)
        topRowH = juce::jmax (0, content.getHeight() - kMinChainH);

    topRowH = juce::jlimit (160, content.getHeight(), topRowH);

    auto topRow = takeTop (content, topRowH);

    constexpr int kMinRightPanelW = 260;
    int leftW = (int) (topRow.getWidth() * 0.65f);
    leftW = juce::jlimit (220, juce::jmax (220, topRow.getWidth() - kMinRightPanelW), leftW);

    auto left = takeLeft (topRow, leftW);
    auto right = topRow;

    deviceGroup.setBounds (left);
    monitorGroup.setBounds (right);
    dspChainGroup.setBounds (content);

    {
        auto deviceArea = deviceGroup.getBounds().reduced (12);
        takeTop (deviceArea, 22);

        deviceSelectorViewport.setBounds (deviceArea);

        if (deviceSelector != nullptr)
        {
            // Give the selector enough height so its internal layout doesn't spill out;
            // the viewport will scroll/clip as needed.
            const auto w = juce::jmax (1, deviceArea.getWidth());
            const auto h = juce::jmax (deviceArea.getHeight(), 520);
            deviceSelector->setSize (w, h);
        }
    }

    {
        auto monitorArea = monitorGroup.getBounds().reduced (12);
        takeTop (monitorArea, 22);

        monitorNoteLabel.setBounds (takeTop (monitorArea, 20));
        takeTop (monitorArea, 6);

        auto controls = takeTop (monitorArea, 26);
        monitorEnabledToggle.setBounds (takeLeft (controls, 110));
        muteToggle.setBounds (takeLeft (controls, 70));
        routingModeCombo.setBounds (controls.reduced (0, 2));

        takeTop (monitorArea, 8);
        monitorGainSlider.setBounds (takeTop (monitorArea, 26));
        takeTop (monitorArea, 8);

        auto inRow = takeTop (monitorArea, 26);
        inputMeter.setBounds (takeRight (inRow, 140).reduced (0, 4));
        inputLevelLabel.setBounds (inRow);

        takeTop (monitorArea, 6);

        auto outRow = takeTop (monitorArea, 26);
        outputMeter.setBounds (takeRight (outRow, 140).reduced (0, 4));
        outputLevelLabel.setBounds (outRow);
    }

    {
        auto chainArea = dspChainGroup.getBounds().reduced (12);
        takeTop (chainArea, 22);

        auto chainHeader = takeTop (chainArea, 26);
        globalBypassToggle.setBounds (takeRight (chainHeader, 160).reduced (0, 2));
        dspChainNoteLabel.setBounds (chainHeader);

        takeTop (chainArea, 8);

        constexpr int kCardGap = 14;
        constexpr int kMaxCardWidth = 260;
        constexpr int kMinCardWidth = 160;
        constexpr int kMaxCardHeight = 260;

        auto layoutFootswitch = [&] (juce::Rectangle<int> footArea, juce::Label& stateLabel, FootswitchButton& button)
        {
            const auto stateH = juce::jmin (18, footArea.getHeight());
            stateLabel.setBounds (takeTop (footArea, stateH));

            auto buttonArea = footArea.reduced (0, 2);
            const auto btnSize = juce::jmin (44, juce::jmin (buttonArea.getWidth(), buttonArea.getHeight()));
            button.setBounds (buttonArea.withSizeKeepingCentre (btnSize, btnSize));
        };

        auto layoutOneKnobCard = [&] (EffectCard& card, juce::Label& label, juce::Slider& knob, juce::Label& stateLabel, FootswitchButton& button)
        {
            auto contentAbs = card.getContentBounds().translated (card.getX(), card.getY());
            label.setBounds (takeTop (contentAbs, juce::jmin (18, contentAbs.getHeight())));
            takeTop (contentAbs, 6);

            const auto footH = juce::jmin (66, contentAbs.getHeight());
            auto footArea = takeBottom (contentAbs, footH);
            layoutFootswitch (footArea, stateLabel, button);

            auto knobArea = contentAbs;
            const auto inset = juce::jlimit (4, 12, knobArea.getWidth() / 10);
            knob.setBounds (knobArea.reduced (inset));
        };

        auto layoutTwoKnobCard = [&] (EffectCard& card,
                                     juce::Label& leftLabel,
                                     juce::Label& rightLabel,
                                     juce::Slider& leftKnob,
                                     juce::Slider& rightKnob,
                                     juce::Label& stateLabel,
                                     FootswitchButton& button)
        {
            auto contentAbs = card.getContentBounds().translated (card.getX(), card.getY());

            auto labelsRow = takeTop (contentAbs, juce::jmin (18, contentAbs.getHeight()));
            auto leftHalf = takeLeft (labelsRow, labelsRow.getWidth() / 2);
            leftLabel.setBounds (leftHalf);
            rightLabel.setBounds (labelsRow);

            takeTop (contentAbs, 6);

            const auto footH = juce::jmin (66, contentAbs.getHeight());
            auto footArea = takeBottom (contentAbs, footH);
            layoutFootswitch (footArea, stateLabel, button);

            auto knobsArea = contentAbs;
            const auto inset = juce::jlimit (4, 12, knobsArea.getWidth() / 12);

            if (knobsArea.getWidth() < 200)
            {
                auto topKnob = takeTop (knobsArea, knobsArea.getHeight() / 2);
                leftKnob.setBounds (topKnob.reduced (inset));
                rightKnob.setBounds (knobsArea.reduced (inset));
            }
            else
            {
                auto leftKnobArea = takeLeft (knobsArea, knobsArea.getWidth() / 2);
                leftKnob.setBounds (leftKnobArea.reduced (inset));
                rightKnob.setBounds (knobsArea.reduced (inset));
            }
        };

        auto layoutThreeKnobCard = [&] (EffectCard& card,
                                        juce::Label& bassLabel,
                                        juce::Label& midLabel,
                                        juce::Label& trebleLabel,
                                        juce::Slider& bassKnob,
                                        juce::Slider& midKnob,
                                        juce::Slider& trebleKnob,
                                        juce::Label& stateLabel,
                                        FootswitchButton& button)
        {
            auto contentAbs = card.getContentBounds().translated (card.getX(), card.getY());

            auto labelsRow = takeTop (contentAbs, juce::jmin (18, contentAbs.getHeight()));
            auto colW = labelsRow.getWidth() / 3;
            bassLabel.setBounds (takeLeft (labelsRow, colW));
            midLabel.setBounds (takeLeft (labelsRow, colW));
            trebleLabel.setBounds (labelsRow);

            takeTop (contentAbs, 6);

            const auto footH = juce::jmin (66, contentAbs.getHeight());
            auto footArea = takeBottom (contentAbs, footH);
            layoutFootswitch (footArea, stateLabel, button);

            auto knobsArea = contentAbs;
            const auto inset = juce::jlimit (4, 12, knobsArea.getWidth() / 16);

            if (knobsArea.getWidth() < 240)
            {
                auto topH = knobsArea.getHeight() / 3;
                bassKnob.setBounds (takeTop (knobsArea, topH).reduced (inset));
                midKnob.setBounds (takeTop (knobsArea, topH).reduced (inset));
                trebleKnob.setBounds (knobsArea.reduced (inset));
            }
            else
            {
                auto thirdW = knobsArea.getWidth() / 3;
                bassKnob.setBounds (takeLeft (knobsArea, thirdW).reduced (inset));
                midKnob.setBounds (takeLeft (knobsArea, thirdW).reduced (inset));
                trebleKnob.setBounds (knobsArea.reduced (inset));
            }
        };

        // Pedalboard cards: 3 columns when space allows, otherwise 2+1, otherwise stack vertically.
        constexpr int kCardCount = 3;

        int columns = 1;
        if (chainArea.getWidth() >= (kMinCardWidth * 3 + kCardGap * 2))
            columns = 3;
        else if (chainArea.getWidth() >= (kMinCardWidth * 2 + kCardGap))
            columns = 2;

        const int rows = (kCardCount + columns - 1) / columns;

        const int hGap = (columns > 1) ? juce::jmin (kCardGap, chainArea.getWidth() / (columns * 10)) : 0;
        const int vGap = (rows > 1) ? juce::jmin (kCardGap, chainArea.getHeight() / (rows * 10)) : 0;

        const int minCardWidth = juce::jmin (kMinCardWidth, chainArea.getWidth());
        const int availableW = juce::jmax (0, chainArea.getWidth() - hGap * (columns - 1));
        int cardWidth = availableW / columns;
        cardWidth = juce::jlimit (minCardWidth, kMaxCardWidth, cardWidth);

        const int availableH = juce::jmax (0, chainArea.getHeight() - vGap * (rows - 1));
        const int cardHeight = juce::jmin (kMaxCardHeight, availableH / rows);

        const int totalH = cardHeight * rows + vGap * (rows - 1);
        int y = chainArea.getY() + (chainArea.getHeight() - totalH) / 2;

        EffectCard* cards[kCardCount] { &cleanBoostCard, &overdriveCard, &eqCard };
        int index = 0;

        for (int r = 0; r < rows; ++r)
        {
            const int itemsThisRow = juce::jmin (columns, kCardCount - index);
            const int rowW = cardWidth * itemsThisRow + hGap * (itemsThisRow - 1);
            int x = chainArea.getX() + (chainArea.getWidth() - rowW) / 2;

            for (int c = 0; c < itemsThisRow; ++c)
            {
                cards[index]->setBounds (x, y, cardWidth, cardHeight);
                x += cardWidth + hGap;
                ++index;
            }

            y += cardHeight + vGap;
        }

        layoutOneKnobCard (cleanBoostCard, cleanBoostGainLabel, cleanBoostGainSlider, cleanBoostStateLabel, cleanBoostToggle);
        layoutTwoKnobCard (overdriveCard,
                           overdriveDriveLabel,
                           overdriveLevelLabel,
                           overdriveDriveSlider,
                           overdriveLevelSlider,
                           overdriveStateLabel,
                           overdriveToggle);
        layoutThreeKnobCard (eqCard,
                             eqBassLabel,
                             eqMidLabel,
                             eqTrebleLabel,
                             eqBassSlider,
                             eqMidSlider,
                             eqTrebleSlider,
                             eqStateLabel,
                             eqToggle);
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

        const auto eqOn = eqEnabled.load (std::memory_order_relaxed);
        const auto eqBass = eqBassDb.load (std::memory_order_relaxed);
        const auto eqMid = eqMidDb.load (std::memory_order_relaxed);
        const auto eqTreble = eqTrebleDb.load (std::memory_order_relaxed);

        juce::String chain = bypassed ? "Bypassed"
                                      : "Clean Boost " + juce::String (boostOn ? "ON" : "OFF")
                                            + " | Gain " + juce::String (boostDb, 1) + " dB"
                                            + " -> Overdrive " + juce::String (odOn ? "ON" : "OFF")
                                            + " | Drive " + juce::String (odDrive, 0) + "%"
                                            + " | Level " + juce::String (odLevel, 0) + "%"
                                            + " -> EQ " + juce::String (eqOn ? "ON" : "OFF")
                                            + " | Bass " + juce::String (eqBass, 1) + " dB"
                                            + " | Mid " + juce::String (eqMid, 1) + " dB"
                                            + " | Treble " + juce::String (eqTreble, 1) + " dB";

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
