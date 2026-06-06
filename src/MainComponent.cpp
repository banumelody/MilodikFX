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

    s.compressorEnabled = compressorEnabled.load (std::memory_order_relaxed);
    s.compressorInputGainDb = compressorInputGainDb.load (std::memory_order_relaxed);
    s.compressorThresholdDb = compressorThresholdDb.load (std::memory_order_relaxed);
    s.compressorRatio = compressorRatio.load (std::memory_order_relaxed);
    s.compressorAttackMs = compressorAttackMs.load (std::memory_order_relaxed);
    s.compressorReleaseMs = compressorReleaseMs.load (std::memory_order_relaxed);

    s.reverbEnabled = reverbEnabled.load (std::memory_order_relaxed);
    s.reverbRoomSize = reverbRoomSize.load (std::memory_order_relaxed);
    s.reverbDryWetMix = reverbDryWetMix.load (std::memory_order_relaxed);
    s.reverbDecayTime = reverbDecayTime.load (std::memory_order_relaxed);
    s.reverbWidth = reverbWidth.load (std::memory_order_relaxed);

    s.toneStackEnabled = toneStackEnabled.load (std::memory_order_relaxed);
    s.toneStackBassDb = toneStackBassDb.load (std::memory_order_relaxed);
    s.toneStackMidDb = toneStackMidDb.load (std::memory_order_relaxed);
    s.toneStackTrebleDb = toneStackTrebleDb.load (std::memory_order_relaxed);

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

    const auto compressorAccent = juce::Colour (0xffff6b35);
    const auto reverbAccent = juce::Colour (0xff35b1ff);
    const auto toneStackAccent = juce::Colour (0xff9d4edd);

    compressorInputGainSlider.setValue (p.compressorInputGainDb, juce::dontSendNotification);
    compressorInputGainDb.store (p.compressorInputGainDb, std::memory_order_relaxed);
    if (compressorProcessor != nullptr)
        compressorProcessor->setInputGainDb (p.compressorInputGainDb);
    settingsFile.setValue (kKeyCompressorInputGainDb, (double) p.compressorInputGainDb);

    compressorThresholdSlider.setValue (p.compressorThresholdDb, juce::dontSendNotification);
    compressorThresholdDb.store (p.compressorThresholdDb, std::memory_order_relaxed);
    if (compressorProcessor != nullptr)
        compressorProcessor->setThresholdDb (p.compressorThresholdDb);
    settingsFile.setValue (kKeyCompressorThresholdDb, (double) p.compressorThresholdDb);

    compressorRatioSlider.setValue (p.compressorRatio, juce::dontSendNotification);
    compressorRatio.store (p.compressorRatio, std::memory_order_relaxed);
    if (compressorProcessor != nullptr)
        compressorProcessor->setRatio (p.compressorRatio);
    settingsFile.setValue (kKeyCompressorRatio, (double) p.compressorRatio);

    compressorAttackSlider.setValue (p.compressorAttackMs, juce::dontSendNotification);
    compressorAttackMs.store (p.compressorAttackMs, std::memory_order_relaxed);
    if (compressorProcessor != nullptr)
        compressorProcessor->setAttackMs (p.compressorAttackMs);
    settingsFile.setValue (kKeyCompressorAttackMs, (double) p.compressorAttackMs);

    compressorReleaseSlider.setValue (p.compressorReleaseMs, juce::dontSendNotification);
    compressorReleaseMs.store (p.compressorReleaseMs, std::memory_order_relaxed);
    if (compressorProcessor != nullptr)
        compressorProcessor->setReleaseMs (p.compressorReleaseMs);
    settingsFile.setValue (kKeyCompressorReleaseMs, (double) p.compressorReleaseMs);

    compressorToggle.setToggleState (p.compressorEnabled, juce::dontSendNotification);
    compressorEnabled.store (p.compressorEnabled, std::memory_order_relaxed);
    if (compressorProcessor != nullptr)
        compressorProcessor->setEnabled (p.compressorEnabled);
    settingsFile.setValue (kKeyCompressorEnabled, p.compressorEnabled);
    setEffectStateUi (compressorCard, compressorStateLabel, p.compressorEnabled, compressorAccent);

    reverbRoomSizeSlider.setValue (p.reverbRoomSize, juce::dontSendNotification);
    reverbRoomSize.store (p.reverbRoomSize, std::memory_order_relaxed);
    if (reverbProcessor != nullptr)
        reverbProcessor->setRoomSize (p.reverbRoomSize);
    settingsFile.setValue (kKeyReverbRoomSize, (double) p.reverbRoomSize);

    reverbDryWetSlider.setValue (p.reverbDryWetMix, juce::dontSendNotification);
    reverbDryWetMix.store (p.reverbDryWetMix, std::memory_order_relaxed);
    if (reverbProcessor != nullptr)
        reverbProcessor->setDryWetMix (p.reverbDryWetMix);
    settingsFile.setValue (kKeyReverbDryWetMix, (double) p.reverbDryWetMix);

    reverbDecaySlider.setValue (p.reverbDecayTime, juce::dontSendNotification);
    reverbDecayTime.store (p.reverbDecayTime, std::memory_order_relaxed);
    if (reverbProcessor != nullptr)
        reverbProcessor->setDecayTime (p.reverbDecayTime);
    settingsFile.setValue (kKeyReverbDecayTime, (double) p.reverbDecayTime);

    reverbWidthSlider.setValue (p.reverbWidth, juce::dontSendNotification);
    reverbWidth.store (p.reverbWidth, std::memory_order_relaxed);
    if (reverbProcessor != nullptr)
        reverbProcessor->setWidth (p.reverbWidth);
    settingsFile.setValue (kKeyReverbWidth, (double) p.reverbWidth);

    reverbToggle.setToggleState (p.reverbEnabled, juce::dontSendNotification);
    reverbEnabled.store (p.reverbEnabled, std::memory_order_relaxed);
    if (reverbProcessor != nullptr)
        reverbProcessor->setEnabled (p.reverbEnabled);
    settingsFile.setValue (kKeyReverbEnabled, p.reverbEnabled);
    setEffectStateUi (reverbCard, reverbStateLabel, p.reverbEnabled, reverbAccent);

    toneStackBassSlider.setValue (p.toneStackBassDb, juce::dontSendNotification);
    toneStackBassDb.store (p.toneStackBassDb, std::memory_order_relaxed);
    if (toneStackProcessor != nullptr)
        toneStackProcessor->setBassDb (p.toneStackBassDb);
    settingsFile.setValue (kKeyToneStackBassDb, (double) p.toneStackBassDb);

    toneStackMidSlider.setValue (p.toneStackMidDb, juce::dontSendNotification);
    toneStackMidDb.store (p.toneStackMidDb, std::memory_order_relaxed);
    if (toneStackProcessor != nullptr)
        toneStackProcessor->setMidDb (p.toneStackMidDb);
    settingsFile.setValue (kKeyToneStackMidDb, (double) p.toneStackMidDb);

    toneStackTrebleSlider.setValue (p.toneStackTrebleDb, juce::dontSendNotification);
    toneStackTrebleDb.store (p.toneStackTrebleDb, std::memory_order_relaxed);
    if (toneStackProcessor != nullptr)
        toneStackProcessor->setTrebleDb (p.toneStackTrebleDb);
    settingsFile.setValue (kKeyToneStackTrebleDb, (double) p.toneStackTrebleDb);

    toneStackToggle.setToggleState (p.toneStackEnabled, juce::dontSendNotification);
    toneStackEnabled.store (p.toneStackEnabled, std::memory_order_relaxed);
    if (toneStackProcessor != nullptr)
        toneStackProcessor->setEnabled (p.toneStackEnabled);
    settingsFile.setValue (kKeyToneStackEnabled, p.toneStackEnabled);
    setEffectStateUi (toneStackCard, toneStackStateLabel, p.toneStackEnabled, toneStackAccent);

    markSettingsDirty();
    saveSettingsIfNeeded (true);
}

void MainComponent::refreshPresetList (const juce::String& selectPresetName)
{
    const auto names = presetManager.listPresets();

    juce::String toSelect = selectPresetName;
    if (toSelect.isEmpty() && names.size() > 0)
        toSelect = names[0];

    int selectedIndex = 0;
    for (int i = 0; i < names.size(); ++i)
    {
        if (names[i] == toSelect)
        {
            selectedIndex = i;
            break;
        }
    }

    presetUI.setPresetsList (names, selectedIndex);
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

    double persistedCompressorInputGainDb = 0.0;
    double persistedCompressorThresholdDb = -24.0;
    double persistedCompressorRatio = 4.0;
    double persistedCompressorAttackMs = 10.0;
    double persistedCompressorReleaseMs = 100.0;
    bool persistedCompressorEnabled = true;

    double persistedReverbRoomSize = 0.5;
    double persistedReverbDryWetMix = 0.5;
    double persistedReverbDecayTime = 2.0;
    double persistedReverbWidth = 1.0;
    bool persistedReverbEnabled = true;

    double persistedToneStackBassDb = 0.0;
    double persistedToneStackMidDb = 0.0;
    double persistedToneStackTrebleDb = 0.0;
    bool persistedToneStackEnabled = true;

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

    persistedCompressorInputGainDb = settingsFile.getDoubleValue (kKeyCompressorInputGainDb, 0.0);
    persistedCompressorThresholdDb = settingsFile.getDoubleValue (kKeyCompressorThresholdDb, -24.0);
    persistedCompressorRatio = settingsFile.getDoubleValue (kKeyCompressorRatio, 4.0);
    persistedCompressorAttackMs = settingsFile.getDoubleValue (kKeyCompressorAttackMs, 10.0);
    persistedCompressorReleaseMs = settingsFile.getDoubleValue (kKeyCompressorReleaseMs, 100.0);
    persistedCompressorEnabled = settingsFile.getBoolValue (kKeyCompressorEnabled, true);

    persistedReverbRoomSize = settingsFile.getDoubleValue (kKeyReverbRoomSize, 0.5);
    persistedReverbDryWetMix = settingsFile.getDoubleValue (kKeyReverbDryWetMix, 0.5);
    persistedReverbDecayTime = settingsFile.getDoubleValue (kKeyReverbDecayTime, 2.0);
    persistedReverbWidth = settingsFile.getDoubleValue (kKeyReverbWidth, 1.0);
    persistedReverbEnabled = settingsFile.getBoolValue (kKeyReverbEnabled, true);

    persistedToneStackBassDb = settingsFile.getDoubleValue (kKeyToneStackBassDb, 0.0);
    persistedToneStackMidDb = settingsFile.getDoubleValue (kKeyToneStackMidDb, 0.0);
    persistedToneStackTrebleDb = settingsFile.getDoubleValue (kKeyToneStackTrebleDb, 0.0);
    persistedToneStackEnabled = settingsFile.getBoolValue (kKeyToneStackEnabled, true);

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

    persistedCompressorInputGainDb = juce::jlimit (-12.0, 12.0, persistedCompressorInputGainDb);
    persistedCompressorThresholdDb = juce::jlimit (-60.0, 0.0, persistedCompressorThresholdDb);
    persistedCompressorRatio = juce::jlimit (1.0, 16.0, persistedCompressorRatio);
    persistedCompressorAttackMs = juce::jlimit (0.1, 100.0, persistedCompressorAttackMs);
    persistedCompressorReleaseMs = juce::jlimit (10.0, 1000.0, persistedCompressorReleaseMs);

    persistedReverbRoomSize = juce::jlimit (0.0, 1.0, persistedReverbRoomSize);
    persistedReverbDryWetMix = juce::jlimit (0.0, 1.0, persistedReverbDryWetMix);
    persistedReverbDecayTime = juce::jlimit (0.5, 10.0, persistedReverbDecayTime);
    persistedReverbWidth = juce::jlimit (0.0, 1.0, persistedReverbWidth);

    persistedToneStackBassDb = juce::jlimit (-12.0, 12.0, persistedToneStackBassDb);
    persistedToneStackMidDb = juce::jlimit (-12.0, 12.0, persistedToneStackMidDb);
    persistedToneStackTrebleDb = juce::jlimit (-12.0, 12.0, persistedToneStackTrebleDb);

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

    compressorInputGainDb.store ((float) persistedCompressorInputGainDb, std::memory_order_relaxed);
    compressorThresholdDb.store ((float) persistedCompressorThresholdDb, std::memory_order_relaxed);
    compressorRatio.store ((float) persistedCompressorRatio, std::memory_order_relaxed);
    compressorAttackMs.store ((float) persistedCompressorAttackMs, std::memory_order_relaxed);
    compressorReleaseMs.store ((float) persistedCompressorReleaseMs, std::memory_order_relaxed);
    compressorEnabled.store (persistedCompressorEnabled, std::memory_order_relaxed);

    reverbRoomSize.store ((float) persistedReverbRoomSize, std::memory_order_relaxed);
    reverbDryWetMix.store ((float) persistedReverbDryWetMix, std::memory_order_relaxed);
    reverbDecayTime.store ((float) persistedReverbDecayTime, std::memory_order_relaxed);
    reverbWidth.store ((float) persistedReverbWidth, std::memory_order_relaxed);
    reverbEnabled.store (persistedReverbEnabled, std::memory_order_relaxed);

    toneStackBassDb.store ((float) persistedToneStackBassDb, std::memory_order_relaxed);
    toneStackMidDb.store ((float) persistedToneStackMidDb, std::memory_order_relaxed);
    toneStackTrebleDb.store ((float) persistedToneStackTrebleDb, std::memory_order_relaxed);
    toneStackEnabled.store (persistedToneStackEnabled, std::memory_order_relaxed);

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

    if (auto* processor = audioEngine.getChain().addProcessor (std::make_unique<milodikfx::dsp::CompressorProcessor>()))
        compressorProcessor = dynamic_cast<milodikfx::dsp::CompressorProcessor*> (processor);

    if (auto* processor = audioEngine.getChain().addProcessor (std::make_unique<milodikfx::dsp::ReverbProcessor>()))
        reverbProcessor = dynamic_cast<milodikfx::dsp::ReverbProcessor*> (processor);

    if (auto* processor = audioEngine.getChain().addProcessor (std::make_unique<milodikfx::dsp::ToneStackProcessor>()))
        toneStackProcessor = dynamic_cast<milodikfx::dsp::ToneStackProcessor*> (processor);

    if (compressorProcessor != nullptr)
    {
        compressorProcessor->setInputGainDb ((float) persistedCompressorInputGainDb);
        compressorProcessor->setThresholdDb ((float) persistedCompressorThresholdDb);
        compressorProcessor->setRatio ((float) persistedCompressorRatio);
        compressorProcessor->setAttackMs ((float) persistedCompressorAttackMs);
        compressorProcessor->setReleaseMs ((float) persistedCompressorReleaseMs);
        compressorProcessor->setEnabled (persistedCompressorEnabled);
    }

    if (reverbProcessor != nullptr)
    {
        reverbProcessor->setRoomSize ((float) persistedReverbRoomSize);
        reverbProcessor->setDryWetMix ((float) persistedReverbDryWetMix);
        reverbProcessor->setDecayTime ((float) persistedReverbDecayTime);
        reverbProcessor->setWidth ((float) persistedReverbWidth);
        reverbProcessor->setEnabled (persistedReverbEnabled);
    }

    if (toneStackProcessor != nullptr)
    {
        toneStackProcessor->setBassDb ((float) persistedToneStackBassDb);
        toneStackProcessor->setMidDb ((float) persistedToneStackMidDb);
        toneStackProcessor->setTrebleDb ((float) persistedToneStackTrebleDb);
        toneStackProcessor->setEnabled (persistedToneStackEnabled);
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
    settingsFile.setValue (kKeyCompressorInputGainDb, persistedCompressorInputGainDb);
    settingsFile.setValue (kKeyCompressorThresholdDb, persistedCompressorThresholdDb);
    settingsFile.setValue (kKeyCompressorRatio, persistedCompressorRatio);
    settingsFile.setValue (kKeyCompressorAttackMs, persistedCompressorAttackMs);
    settingsFile.setValue (kKeyCompressorReleaseMs, persistedCompressorReleaseMs);
    settingsFile.setValue (kKeyCompressorEnabled, persistedCompressorEnabled);
    settingsFile.setValue (kKeyReverbRoomSize, persistedReverbRoomSize);
    settingsFile.setValue (kKeyReverbDryWetMix, persistedReverbDryWetMix);
    settingsFile.setValue (kKeyReverbDecayTime, persistedReverbDecayTime);
    settingsFile.setValue (kKeyReverbWidth, persistedReverbWidth);
    settingsFile.setValue (kKeyReverbEnabled, persistedReverbEnabled);
    settingsFile.setValue (kKeyToneStackBassDb, persistedToneStackBassDb);
    settingsFile.setValue (kKeyToneStackMidDb, persistedToneStackMidDb);
    settingsFile.setValue (kKeyToneStackTrebleDb, persistedToneStackTrebleDb);
    settingsFile.setValue (kKeyToneStackEnabled, persistedToneStackEnabled);
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

    presetUI.onSavePressed = [this]
    {
        const auto initial = presetUI.getSelectedPresetName().isNotEmpty() ? presetUI.getSelectedPresetName() : juce::String ("My Preset");
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
    addAndMakeVisible (presetUI);

    presetUI.onLoadPressed = [this]
    {
        const auto name = presetUI.getSelectedPresetName();
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

    presetUI.onDeletePressed = [this]
    {
        const auto name = presetUI.getSelectedPresetName();
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

    presetUI.onPresetSelected = [this] (int index)
    {
        if (index >= 0)
        {
            const auto name = presetUI.getSelectedPresetName();
            if (name.isNotEmpty())
            {
                settingsFile.setValue (kKeySelectedPresetName, juce::var (name));
                markSettingsDirty();
            }
        }
    };

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

    compressorCard.setTitle ("COMPRESSOR");
    compressorCard.setAccentColour (juce::Colour (0xffff6b35));
    addAndMakeVisible (compressorCard);

    reverbCard.setTitle ("REVERB");
    reverbCard.setAccentColour (juce::Colour (0xff35b1ff));
    addAndMakeVisible (reverbCard);

    toneStackCard.setTitle ("TONE STACK");
    toneStackCard.setAccentColour (juce::Colour (0xff9d4edd));
    addAndMakeVisible (toneStackCard);

    cleanBoostCard.setEnabledState (persistedCleanBoostEnabled);
    overdriveCard.setEnabledState (persistedOverdriveEnabled);
    eqCard.setEnabledState (persistedEqEnabled);
    compressorCard.setEnabledState (persistedCompressorEnabled);
    reverbCard.setEnabledState (persistedReverbEnabled);
    toneStackCard.setEnabledState (persistedToneStackEnabled);

    dspChainNoteLabel.setText ("DSP Chain: Clean Boost -> Overdrive -> EQ -> Compressor -> Reverb -> Tone Stack", juce::dontSendNotification);
    dspChainNoteLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (dspChainNoteLabel);

    monitorUI.setMonitorEnabled (persistedMonitor);
    monitorUI.setMuted (persistedMute);
    monitorUI.setRoutingMode (persistedRouting);
    monitorUI.setGainDb ((float) persistedGainDb);

    monitorUI.onMonitorToggle = [this] (bool enabled)
    {
        monitorEnabled.store (enabled, std::memory_order_relaxed);
        settingsFile.setValue (kKeyMonitorEnabled, enabled);
        markSettingsDirty();
    };

    monitorUI.onMuteToggle = [this] (bool isMuted)
    {
        muted.store (isMuted, std::memory_order_relaxed);
        settingsFile.setValue (kKeyMuted, isMuted);
        markSettingsDirty();
    };

    monitorUI.onGainChange = [this] (float db)
    {
        monitorGainDb.store (db, std::memory_order_relaxed);
        monitorGainLinear.store (juce::Decibels::decibelsToGain (db), std::memory_order_relaxed);
        settingsFile.setValue (kKeyMonitorGainDb, (double) db);
        markSettingsDirty();
    };

    monitorUI.onRoutingChange = [this] (int modeId)
    {
        routingMode.store (modeId, std::memory_order_relaxed);
        settingsFile.setValue (kKeyRoutingModeId, modeId);
        markSettingsDirty();
    };
    addAndMakeVisible (monitorUI);

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
    eqBassLabel.setMinimumHorizontalScale (0.75f);
    addAndMakeVisible (eqBassLabel);

    eqMidLabel.setText ("Mid", juce::dontSendNotification);
    eqMidLabel.setJustificationType (juce::Justification::centred);
    eqMidLabel.setMinimumHorizontalScale (0.75f);
    addAndMakeVisible (eqMidLabel);

    eqTrebleLabel.setText ("Treble", juce::dontSendNotification);
    eqTrebleLabel.setJustificationType (juce::Justification::centred);
    eqTrebleLabel.setMinimumHorizontalScale (0.75f);
    addAndMakeVisible (eqTrebleLabel);

    eqStateLabel.setJustificationType (juce::Justification::centred);
    eqStateLabel.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    addAndMakeVisible (eqStateLabel);

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
    eqBassSlider.setNumDecimalPlacesToDisplay (1);
    eqBassSlider.setDoubleClickReturnValue (true, 0.0);
    eqBassSlider.setPopupDisplayEnabled (true, false, this);
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
    eqMidSlider.setNumDecimalPlacesToDisplay (1);
    eqMidSlider.setDoubleClickReturnValue (true, 0.0);
    eqMidSlider.setPopupDisplayEnabled (true, false, this);
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
    eqTrebleSlider.setNumDecimalPlacesToDisplay (1);
    eqTrebleSlider.setDoubleClickReturnValue (true, 0.0);
    eqTrebleSlider.setPopupDisplayEnabled (true, false, this);
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

    // Compressor controls
    const auto compressorAccent = juce::Colour (0xffff6b35);

    compressorInputGainLabel.setText ("Input Gain", juce::dontSendNotification);
    compressorInputGainLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (compressorInputGainLabel);

    configureKnob (compressorInputGainSlider, compressorAccent);
    compressorInputGainSlider.setTextValueSuffix (" dB");
    compressorInputGainSlider.setNumDecimalPlacesToDisplay (1);
    compressorInputGainSlider.setDoubleClickReturnValue (true, 0.0);
    compressorInputGainSlider.setPopupDisplayEnabled (true, false, this);
    compressorInputGainSlider.setRange (-12.0, 12.0, 0.1);
    compressorInputGainSlider.setValue (persistedCompressorInputGainDb, juce::dontSendNotification);
    compressorInputGainSlider.onValueChange = [this]
    {
        const auto db = (float) compressorInputGainSlider.getValue();
        compressorInputGainDb.store (db, std::memory_order_relaxed);
        if (compressorProcessor != nullptr)
            compressorProcessor->setInputGainDb (db);

        settingsFile.setValue (kKeyCompressorInputGainDb, (double) db);
        markSettingsDirty();
    };
    addAndMakeVisible (compressorInputGainSlider);

    compressorThresholdLabel.setText ("Threshold", juce::dontSendNotification);
    compressorThresholdLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (compressorThresholdLabel);

    configureKnob (compressorThresholdSlider, compressorAccent);
    compressorThresholdSlider.setTextValueSuffix (" dB");
    compressorThresholdSlider.setNumDecimalPlacesToDisplay (1);
    compressorThresholdSlider.setDoubleClickReturnValue (true, -24.0);
    compressorThresholdSlider.setPopupDisplayEnabled (true, false, this);
    compressorThresholdSlider.setRange (-60.0, 0.0, 0.1);
    compressorThresholdSlider.setValue (persistedCompressorThresholdDb, juce::dontSendNotification);
    compressorThresholdSlider.onValueChange = [this]
    {
        const auto db = (float) compressorThresholdSlider.getValue();
        compressorThresholdDb.store (db, std::memory_order_relaxed);
        if (compressorProcessor != nullptr)
            compressorProcessor->setThresholdDb (db);

        settingsFile.setValue (kKeyCompressorThresholdDb, (double) db);
        markSettingsDirty();
    };
    addAndMakeVisible (compressorThresholdSlider);

    compressorRatioLabel.setText ("Ratio", juce::dontSendNotification);
    compressorRatioLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (compressorRatioLabel);

    configureKnob (compressorRatioSlider, compressorAccent);
    compressorRatioSlider.setTextValueSuffix (":1");
    compressorRatioSlider.setNumDecimalPlacesToDisplay (1);
    compressorRatioSlider.setDoubleClickReturnValue (true, 4.0);
    compressorRatioSlider.setPopupDisplayEnabled (true, false, this);
    compressorRatioSlider.setRange (1.0, 16.0, 0.1);
    compressorRatioSlider.setValue (persistedCompressorRatio, juce::dontSendNotification);
    compressorRatioSlider.onValueChange = [this]
    {
        const auto ratio = (float) compressorRatioSlider.getValue();
        compressorRatio.store (ratio, std::memory_order_relaxed);
        if (compressorProcessor != nullptr)
            compressorProcessor->setRatio (ratio);

        settingsFile.setValue (kKeyCompressorRatio, (double) ratio);
        markSettingsDirty();
    };
    addAndMakeVisible (compressorRatioSlider);

    compressorAttackLabel.setText ("Attack", juce::dontSendNotification);
    compressorAttackLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (compressorAttackLabel);

    configureKnob (compressorAttackSlider, compressorAccent);
    compressorAttackSlider.setTextValueSuffix (" ms");
    compressorAttackSlider.setNumDecimalPlacesToDisplay (1);
    compressorAttackSlider.setDoubleClickReturnValue (true, 10.0);
    compressorAttackSlider.setPopupDisplayEnabled (true, false, this);
    compressorAttackSlider.setRange (0.1, 100.0, 0.1);
    compressorAttackSlider.setValue (persistedCompressorAttackMs, juce::dontSendNotification);
    compressorAttackSlider.onValueChange = [this]
    {
        const auto ms = (float) compressorAttackSlider.getValue();
        compressorAttackMs.store (ms, std::memory_order_relaxed);
        if (compressorProcessor != nullptr)
            compressorProcessor->setAttackMs (ms);

        settingsFile.setValue (kKeyCompressorAttackMs, (double) ms);
        markSettingsDirty();
    };
    addAndMakeVisible (compressorAttackSlider);

    compressorReleaseLabel.setText ("Release", juce::dontSendNotification);
    compressorReleaseLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (compressorReleaseLabel);

    configureKnob (compressorReleaseSlider, compressorAccent);
    compressorReleaseSlider.setTextValueSuffix (" ms");
    compressorReleaseSlider.setNumDecimalPlacesToDisplay (1);
    compressorReleaseSlider.setDoubleClickReturnValue (true, 100.0);
    compressorReleaseSlider.setPopupDisplayEnabled (true, false, this);
    compressorReleaseSlider.setRange (10.0, 1000.0, 1.0);
    compressorReleaseSlider.setValue (persistedCompressorReleaseMs, juce::dontSendNotification);
    compressorReleaseSlider.onValueChange = [this]
    {
        const auto ms = (float) compressorReleaseSlider.getValue();
        compressorReleaseMs.store (ms, std::memory_order_relaxed);
        if (compressorProcessor != nullptr)
            compressorProcessor->setReleaseMs (ms);

        settingsFile.setValue (kKeyCompressorReleaseMs, (double) ms);
        markSettingsDirty();
    };
    addAndMakeVisible (compressorReleaseSlider);

    compressorStateLabel.setJustificationType (juce::Justification::centred);
    compressorStateLabel.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    addAndMakeVisible (compressorStateLabel);

    compressorToggle.setButtonText ({});
    compressorToggle.setAccentColour (compressorAccent);
    compressorToggle.setToggleState (persistedCompressorEnabled, juce::dontSendNotification);
    setEffectStateUi (compressorCard, compressorStateLabel, persistedCompressorEnabled, compressorAccent);

    compressorToggle.onClick = [this, compressorAccent, setEffectStateUi]
    {
        const auto v = compressorToggle.getToggleState();
        compressorEnabled.store (v, std::memory_order_relaxed);
        if (compressorProcessor != nullptr)
            compressorProcessor->setEnabled (v);

        settingsFile.setValue (kKeyCompressorEnabled, v);
        markSettingsDirty();

        setEffectStateUi (compressorCard, compressorStateLabel, v, compressorAccent);
    };
    addAndMakeVisible (compressorToggle);

    // Reverb controls
    const auto reverbAccent = juce::Colour (0xff35b1ff);

    reverbRoomSizeLabel.setText ("Room Size", juce::dontSendNotification);
    reverbRoomSizeLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (reverbRoomSizeLabel);

    configureKnob (reverbRoomSizeSlider, reverbAccent);
    reverbRoomSizeSlider.setTextValueSuffix ("");
    reverbRoomSizeSlider.setNumDecimalPlacesToDisplay (2);
    reverbRoomSizeSlider.setDoubleClickReturnValue (true, 0.5);
    reverbRoomSizeSlider.setPopupDisplayEnabled (true, false, this);
    reverbRoomSizeSlider.setRange (0.0, 1.0, 0.01);
    reverbRoomSizeSlider.setValue (persistedReverbRoomSize, juce::dontSendNotification);
    reverbRoomSizeSlider.onValueChange = [this]
    {
        const auto size = (float) reverbRoomSizeSlider.getValue();
        reverbRoomSize.store (size, std::memory_order_relaxed);
        if (reverbProcessor != nullptr)
            reverbProcessor->setRoomSize (size);

        settingsFile.setValue (kKeyReverbRoomSize, (double) size);
        markSettingsDirty();
    };
    addAndMakeVisible (reverbRoomSizeSlider);

    reverbDryWetLabel.setText ("Dry/Wet", juce::dontSendNotification);
    reverbDryWetLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (reverbDryWetLabel);

    configureKnob (reverbDryWetSlider, reverbAccent);
    reverbDryWetSlider.setTextValueSuffix ("");
    reverbDryWetSlider.setNumDecimalPlacesToDisplay (2);
    reverbDryWetSlider.setDoubleClickReturnValue (true, 0.5);
    reverbDryWetSlider.setPopupDisplayEnabled (true, false, this);
    reverbDryWetSlider.setRange (0.0, 1.0, 0.01);
    reverbDryWetSlider.setValue (persistedReverbDryWetMix, juce::dontSendNotification);
    reverbDryWetSlider.onValueChange = [this]
    {
        const auto mix = (float) reverbDryWetSlider.getValue();
        reverbDryWetMix.store (mix, std::memory_order_relaxed);
        if (reverbProcessor != nullptr)
            reverbProcessor->setDryWetMix (mix);

        settingsFile.setValue (kKeyReverbDryWetMix, (double) mix);
        markSettingsDirty();
    };
    addAndMakeVisible (reverbDryWetSlider);

    reverbDecayLabel.setText ("Decay", juce::dontSendNotification);
    reverbDecayLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (reverbDecayLabel);

    configureKnob (reverbDecaySlider, reverbAccent);
    reverbDecaySlider.setTextValueSuffix (" s");
    reverbDecaySlider.setNumDecimalPlacesToDisplay (2);
    reverbDecaySlider.setDoubleClickReturnValue (true, 2.0);
    reverbDecaySlider.setPopupDisplayEnabled (true, false, this);
    reverbDecaySlider.setRange (0.5, 10.0, 0.1);
    reverbDecaySlider.setValue (persistedReverbDecayTime, juce::dontSendNotification);
    reverbDecaySlider.onValueChange = [this]
    {
        const auto time = (float) reverbDecaySlider.getValue();
        reverbDecayTime.store (time, std::memory_order_relaxed);
        if (reverbProcessor != nullptr)
            reverbProcessor->setDecayTime (time);

        settingsFile.setValue (kKeyReverbDecayTime, (double) time);
        markSettingsDirty();
    };
    addAndMakeVisible (reverbDecaySlider);

    reverbWidthLabel.setText ("Width", juce::dontSendNotification);
    reverbWidthLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (reverbWidthLabel);

    configureKnob (reverbWidthSlider, reverbAccent);
    reverbWidthSlider.setTextValueSuffix ("");
    reverbWidthSlider.setNumDecimalPlacesToDisplay (2);
    reverbWidthSlider.setDoubleClickReturnValue (true, 1.0);
    reverbWidthSlider.setPopupDisplayEnabled (true, false, this);
    reverbWidthSlider.setRange (0.0, 1.0, 0.01);
    reverbWidthSlider.setValue (persistedReverbWidth, juce::dontSendNotification);
    reverbWidthSlider.onValueChange = [this]
    {
        const auto width = (float) reverbWidthSlider.getValue();
        reverbWidth.store (width, std::memory_order_relaxed);
        if (reverbProcessor != nullptr)
            reverbProcessor->setWidth (width);

        settingsFile.setValue (kKeyReverbWidth, (double) width);
        markSettingsDirty();
    };
    addAndMakeVisible (reverbWidthSlider);

    reverbStateLabel.setJustificationType (juce::Justification::centred);
    reverbStateLabel.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    addAndMakeVisible (reverbStateLabel);

    reverbToggle.setButtonText ({});
    reverbToggle.setAccentColour (reverbAccent);
    reverbToggle.setToggleState (persistedReverbEnabled, juce::dontSendNotification);
    setEffectStateUi (reverbCard, reverbStateLabel, persistedReverbEnabled, reverbAccent);

    reverbToggle.onClick = [this, reverbAccent, setEffectStateUi]
    {
        const auto v = reverbToggle.getToggleState();
        reverbEnabled.store (v, std::memory_order_relaxed);
        if (reverbProcessor != nullptr)
            reverbProcessor->setEnabled (v);

        settingsFile.setValue (kKeyReverbEnabled, v);
        markSettingsDirty();

        setEffectStateUi (reverbCard, reverbStateLabel, v, reverbAccent);
    };
    addAndMakeVisible (reverbToggle);

    // Tone Stack controls
    const auto toneStackAccent = juce::Colour (0xff9d4edd);

    toneStackBassLabel.setText ("Bass", juce::dontSendNotification);
    toneStackBassLabel.setJustificationType (juce::Justification::centred);
    toneStackBassLabel.setMinimumHorizontalScale (0.75f);
    addAndMakeVisible (toneStackBassLabel);

    configureKnob (toneStackBassSlider, toneStackAccent);
    toneStackBassSlider.setTextValueSuffix (" dB");
    toneStackBassSlider.setNumDecimalPlacesToDisplay (1);
    toneStackBassSlider.setDoubleClickReturnValue (true, 0.0);
    toneStackBassSlider.setPopupDisplayEnabled (true, false, this);
    toneStackBassSlider.setRange (-12.0, 12.0, 0.1);
    toneStackBassSlider.setValue (persistedToneStackBassDb, juce::dontSendNotification);
    toneStackBassSlider.onValueChange = [this]
    {
        const auto db = (float) toneStackBassSlider.getValue();
        toneStackBassDb.store (db, std::memory_order_relaxed);
        if (toneStackProcessor != nullptr)
            toneStackProcessor->setBassDb (db);

        settingsFile.setValue (kKeyToneStackBassDb, (double) db);
        markSettingsDirty();
    };
    addAndMakeVisible (toneStackBassSlider);

    toneStackMidLabel.setText ("Mid", juce::dontSendNotification);
    toneStackMidLabel.setJustificationType (juce::Justification::centred);
    toneStackMidLabel.setMinimumHorizontalScale (0.75f);
    addAndMakeVisible (toneStackMidLabel);

    configureKnob (toneStackMidSlider, toneStackAccent);
    toneStackMidSlider.setTextValueSuffix (" dB");
    toneStackMidSlider.setNumDecimalPlacesToDisplay (1);
    toneStackMidSlider.setDoubleClickReturnValue (true, 0.0);
    toneStackMidSlider.setPopupDisplayEnabled (true, false, this);
    toneStackMidSlider.setRange (-12.0, 12.0, 0.1);
    toneStackMidSlider.setValue (persistedToneStackMidDb, juce::dontSendNotification);
    toneStackMidSlider.onValueChange = [this]
    {
        const auto db = (float) toneStackMidSlider.getValue();
        toneStackMidDb.store (db, std::memory_order_relaxed);
        if (toneStackProcessor != nullptr)
            toneStackProcessor->setMidDb (db);

        settingsFile.setValue (kKeyToneStackMidDb, (double) db);
        markSettingsDirty();
    };
    addAndMakeVisible (toneStackMidSlider);

    toneStackTrebleLabel.setText ("Treble", juce::dontSendNotification);
    toneStackTrebleLabel.setJustificationType (juce::Justification::centred);
    toneStackTrebleLabel.setMinimumHorizontalScale (0.75f);
    addAndMakeVisible (toneStackTrebleLabel);

    configureKnob (toneStackTrebleSlider, toneStackAccent);
    toneStackTrebleSlider.setTextValueSuffix (" dB");
    toneStackTrebleSlider.setNumDecimalPlacesToDisplay (1);
    toneStackTrebleSlider.setDoubleClickReturnValue (true, 0.0);
    toneStackTrebleSlider.setPopupDisplayEnabled (true, false, this);
    toneStackTrebleSlider.setRange (-12.0, 12.0, 0.1);
    toneStackTrebleSlider.setValue (persistedToneStackTrebleDb, juce::dontSendNotification);
    toneStackTrebleSlider.onValueChange = [this]
    {
        const auto db = (float) toneStackTrebleSlider.getValue();
        toneStackTrebleDb.store (db, std::memory_order_relaxed);
        if (toneStackProcessor != nullptr)
            toneStackProcessor->setTrebleDb (db);

        settingsFile.setValue (kKeyToneStackTrebleDb, (double) db);
        markSettingsDirty();
    };
    addAndMakeVisible (toneStackTrebleSlider);

    toneStackStateLabel.setJustificationType (juce::Justification::centred);
    toneStackStateLabel.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    addAndMakeVisible (toneStackStateLabel);

    toneStackToggle.setButtonText ({});
    toneStackToggle.setAccentColour (toneStackAccent);
    toneStackToggle.setToggleState (persistedToneStackEnabled, juce::dontSendNotification);
    setEffectStateUi (toneStackCard, toneStackStateLabel, persistedToneStackEnabled, toneStackAccent);

    toneStackToggle.onClick = [this, toneStackAccent, setEffectStateUi]
    {
        const auto v = toneStackToggle.getToggleState();
        toneStackEnabled.store (v, std::memory_order_relaxed);
        if (toneStackProcessor != nullptr)
            toneStackProcessor->setEnabled (v);

        settingsFile.setValue (kKeyToneStackEnabled, v);
        markSettingsDirty();

        setEffectStateUi (toneStackCard, toneStackStateLabel, v, toneStackAccent);
    };
    addAndMakeVisible (toneStackToggle);

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

    // Sprint 5: presets
    milodikfx::preset::PresetState defaultPreset;
    presetManager.ensurePresetExists ("Default Clean", defaultPreset);
    refreshPresetList (persistedPresetName);

    milodikfx::preset::PresetState startupPreset;
    const auto startupName = presetUI.getSelectedPresetName();
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

    // Header section: title, version (~50px)
    auto header = takeTop (bounds, 50);
    auto titleArea = takeLeft (header, 220);
    titleLabel.setBounds (takeTop (titleArea, 28));
    versionLabel.setBounds (titleArea);
    retryAudioButton.setBounds (takeRight (header, 120));
    deviceStatusLabel.setBounds (header);

    // Preset bar
    auto presetBar = takeTop (bounds, 44);
    {
        presetUI.setBounds (presetBar.reduced (0, 6));
    }

    auto content = bounds;

    // Device group (~80px)
    auto deviceArea = takeTop (content, 80);
    deviceGroup.setBounds (deviceArea);
    {
        auto internalArea = deviceArea.reduced (12);
        takeTop (internalArea, 22);
        deviceSelectorViewport.setBounds (internalArea);

        if (deviceSelector != nullptr)
        {
            const auto w = juce::jmax (1, internalArea.getWidth());
            const auto h = juce::jmax (internalArea.getHeight(), 520);
            deviceSelector->setSize (w, h);
        }
    }

    // Monitor group (~120px)
    auto monitorArea = takeTop (content, 120);
    monitorGroup.setBounds (monitorArea);
    {
        auto internalArea = monitorArea.reduced (12);
        takeTop (internalArea, 22);
        monitorUI.setBounds (internalArea);
    }

    // DSP Chain group - remaining space (should be ~400px for 1200x700)
    dspChainGroup.setBounds (content);

    {
        auto chainArea = dspChainGroup.getBounds().reduced (12);
        takeTop (chainArea, 22);

        auto chainHeader = takeTop (chainArea, 26);
        globalBypassToggle.setBounds (takeRight (chainHeader, 160).reduced (0, 2));
        dspChainNoteLabel.setBounds (chainHeader);

        takeTop (chainArea, 8);

        // Improved card layout: 2x3 grid with better proportions
        constexpr int kCardGap = 12;  // Gap between cards
        constexpr int kMinCardW = 160;
        constexpr int kMinCardH = 140;

        // Determine layout columns based on available space
        int columns = 1;
        if (chainArea.getWidth() >= (kMinCardW * 3 + kCardGap * 2))
            columns = 3;
        else if (chainArea.getWidth() >= (kMinCardW * 2 + kCardGap))
            columns = 2;

        constexpr int kCardCount = 6;
        const int rows = (kCardCount + columns - 1) / columns;

        const int hGap = (columns > 1) ? juce::jmin (kCardGap, chainArea.getWidth() / (columns * 10)) : 0;
        const int vGap = (rows > 1) ? juce::jmin (kCardGap, chainArea.getHeight() / (rows * 10)) : 0;

        const int availableForCards = juce::jmax (0, chainArea.getWidth() - hGap * (columns - 1));
        int perCardW = availableForCards / columns;
        perCardW = juce::jlimit (kMinCardW, 400, perCardW);

        const int availableForCardsH = juce::jmax (0, chainArea.getHeight() - vGap * (rows - 1));
        int perCardH = availableForCardsH / rows;
        perCardH = juce::jlimit (kMinCardH, 250, perCardH);

        const int totalH = perCardH * rows + vGap * (rows - 1);
        int y = chainArea.getY() + (chainArea.getHeight() - totalH) / 2;

        EffectCard* cards[kCardCount] { &cleanBoostCard, &overdriveCard, &eqCard, &compressorCard, &reverbCard, &toneStackCard };
        int index = 0;

        for (int r = 0; r < rows; ++r)
        {
            const int itemsThisRow = juce::jmin (columns, kCardCount - index);
            const int rowW = perCardW * itemsThisRow + hGap * (itemsThisRow - 1);
            int x = chainArea.getX() + (chainArea.getWidth() - rowW) / 2;

            for (int c = 0; c < itemsThisRow; ++c)
            {
                cards[index]->setBounds (x, y, perCardW, perCardH);
                x += perCardW + hGap;
                ++index;
            }

            y += perCardH + vGap;
        }

        auto layoutFootswitch = [&] (juce::Rectangle<int> footArea, juce::Label& stateLabel, FootswitchButton& button)
        {
            const auto stateH = juce::jmin (16, footArea.getHeight() / 3);
            stateLabel.setBounds (takeTop (footArea, stateH));
            takeTop (footArea, 4);

            auto buttonArea = footArea.reduced (0, 2);
            const auto btnSize = juce::jmin (40, juce::jmin (buttonArea.getWidth(), buttonArea.getHeight()));
            button.setBounds (buttonArea.withSizeKeepingCentre (btnSize, btnSize));
        };

        auto layoutOneKnobCard = [&] (EffectCard& card, juce::Label& label, juce::Slider& knob, juce::Label& stateLabel, FootswitchButton& button)
        {
            auto contentAbs = card.getContentBounds().translated (card.getX(), card.getY());
            label.setBounds (takeTop (contentAbs, 18));
            takeTop (contentAbs, 6);

            const auto footH = juce::jmin (72, contentAbs.getHeight());
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

            auto labelsRow = takeTop (contentAbs, 18);
            auto leftHalf = takeLeft (labelsRow, labelsRow.getWidth() / 2);
            leftLabel.setBounds (leftHalf);
            rightLabel.setBounds (labelsRow);

            takeTop (contentAbs, 6);

            const auto footH = juce::jmin (72, contentAbs.getHeight());
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

            auto labelsRow = takeTop (contentAbs, 18);
            const auto colW = labelsRow.getWidth() / 3;
            bassLabel.setBounds (takeLeft (labelsRow, colW).reduced (2, 0));
            midLabel.setBounds (takeLeft (labelsRow, colW).reduced (2, 0));
            trebleLabel.setBounds (labelsRow.reduced (2, 0));

            takeTop (contentAbs, 6);

            const auto footH = juce::jmin (60, juce::jmax (50, contentAbs.getHeight() / 3));
            auto footArea = takeBottom (contentAbs, footH);
            layoutFootswitch (footArea, stateLabel, button);

            auto knobsArea = contentAbs;
            const auto minDim = juce::jmin (knobsArea.getWidth(), knobsArea.getHeight());
            const auto inset = juce::jlimit (2, 8, minDim / 16);

            const int gap = juce::jmin (8, juce::jmax (3, knobsArea.getWidth() / 28));
            const int w = juce::jmax (0, (knobsArea.getWidth() - gap * 2) / 3);

            bassKnob.setBounds (takeLeft (knobsArea, w).reduced (inset));
            takeLeft (knobsArea, gap);
            midKnob.setBounds (takeLeft (knobsArea, w).reduced (inset));
            takeLeft (knobsArea, gap);
            trebleKnob.setBounds (knobsArea.reduced (inset));
        };

        auto layoutFourKnobCard = [&] (EffectCard& card,
                                       juce::Label& label1,
                                       juce::Label& label2,
                                       juce::Label& label3,
                                       juce::Label& label4,
                                       juce::Slider& knob1,
                                       juce::Slider& knob2,
                                       juce::Slider& knob3,
                                       juce::Slider& knob4,
                                       juce::Label& stateLabel,
                                       FootswitchButton& button)
        {
            auto contentAbs = card.getContentBounds().translated (card.getX(), card.getY());

            auto labelsRow = takeTop (contentAbs, 18);
            const auto colW = labelsRow.getWidth() / 4;
            label1.setBounds (takeLeft (labelsRow, colW).reduced (1, 0));
            label2.setBounds (takeLeft (labelsRow, colW).reduced (1, 0));
            label3.setBounds (takeLeft (labelsRow, colW).reduced (1, 0));
            label4.setBounds (labelsRow.reduced (1, 0));

            takeTop (contentAbs, 6);

            const auto footH = juce::jmin (60, juce::jmax (50, contentAbs.getHeight() / 3));
            auto footArea = takeBottom (contentAbs, footH);
            layoutFootswitch (footArea, stateLabel, button);

            auto knobsArea = contentAbs;
            const auto minDim = juce::jmin (knobsArea.getWidth(), knobsArea.getHeight());
            const auto inset = juce::jlimit (2, 8, minDim / 16);

            const int gap = juce::jmin (6, juce::jmax (2, knobsArea.getWidth() / 40));
            const int w = juce::jmax (0, (knobsArea.getWidth() - gap * 3) / 4);

            knob1.setBounds (takeLeft (knobsArea, w).reduced (inset));
            takeLeft (knobsArea, gap);
            knob2.setBounds (takeLeft (knobsArea, w).reduced (inset));
            takeLeft (knobsArea, gap);
            knob3.setBounds (takeLeft (knobsArea, w).reduced (inset));
            takeLeft (knobsArea, gap);
            knob4.setBounds (knobsArea.reduced (inset));
        };

        auto layoutFiveKnobCard = [&] (EffectCard& card,
                                       juce::Label& label1,
                                       juce::Label& label2,
                                       juce::Label& label3,
                                       juce::Label& label4,
                                       juce::Label& label5,
                                       juce::Slider& knob1,
                                       juce::Slider& knob2,
                                       juce::Slider& knob3,
                                       juce::Slider& knob4,
                                       juce::Slider& knob5,
                                       juce::Label& stateLabel,
                                       FootswitchButton& button)
        {
            auto contentAbs = card.getContentBounds().translated (card.getX(), card.getY());

            if (contentAbs.getWidth() < 180)
            {
                auto labelsRow1 = takeTop (contentAbs, 16);
                const auto colW1 = labelsRow1.getWidth() / 3;
                label1.setBounds (takeLeft (labelsRow1, colW1).reduced (1, 0));
                label2.setBounds (takeLeft (labelsRow1, colW1).reduced (1, 0));
                label3.setBounds (labelsRow1.reduced (1, 0));

                auto labelsRow2 = takeTop (contentAbs, 16);
                const auto colW2 = labelsRow2.getWidth() / 2;
                label4.setBounds (takeLeft (labelsRow2, colW2).reduced (1, 0));
                label5.setBounds (labelsRow2.reduced (1, 0));
            }
            else
            {
                auto labelsRow = takeTop (contentAbs, 18);
                const auto colW = labelsRow.getWidth() / 5;
                label1.setBounds (takeLeft (labelsRow, colW).reduced (1, 0));
                label2.setBounds (takeLeft (labelsRow, colW).reduced (1, 0));
                label3.setBounds (takeLeft (labelsRow, colW).reduced (1, 0));
                label4.setBounds (takeLeft (labelsRow, colW).reduced (1, 0));
                label5.setBounds (labelsRow.reduced (1, 0));
            }

            takeTop (contentAbs, 4);

            const auto footH = juce::jmin (60, juce::jmax (50, contentAbs.getHeight() / 3));
            auto footArea = takeBottom (contentAbs, footH);
            layoutFootswitch (footArea, stateLabel, button);

            auto knobsArea = contentAbs;
            const auto minDim = juce::jmin (knobsArea.getWidth(), knobsArea.getHeight());
            const auto inset = juce::jlimit (2, 6, minDim / 16);

            if (knobsArea.getWidth() < 180)
            {
                const int gap = juce::jmin (4, juce::jmax (1, knobsArea.getWidth() / 40));
                const int w = juce::jmax (0, (knobsArea.getWidth() - gap * 2) / 3);

                auto topRow = takeTop (knobsArea, knobsArea.getHeight() / 2);
                knob1.setBounds (takeLeft (topRow, w).reduced (inset));
                takeLeft (topRow, gap);
                knob2.setBounds (takeLeft (topRow, w).reduced (inset));
                takeLeft (topRow, gap);
                knob3.setBounds (topRow.reduced (inset));

                takeTop (knobsArea, 3);

                const int w2 = juce::jmax (0, (knobsArea.getWidth() - gap * 1) / 2);
                knob4.setBounds (takeLeft (knobsArea, w2).reduced (inset));
                takeLeft (knobsArea, gap);
                knob5.setBounds (knobsArea.reduced (inset));
            }
            else
            {
                const int gap = juce::jmin (5, juce::jmax (2, knobsArea.getWidth() / 40));
                const int w = juce::jmax (0, (knobsArea.getWidth() - gap * 4) / 5);

                knob1.setBounds (takeLeft (knobsArea, w).reduced (inset));
                takeLeft (knobsArea, gap);
                knob2.setBounds (takeLeft (knobsArea, w).reduced (inset));
                takeLeft (knobsArea, gap);
                knob3.setBounds (takeLeft (knobsArea, w).reduced (inset));
                takeLeft (knobsArea, gap);
                knob4.setBounds (takeLeft (knobsArea, w).reduced (inset));
                takeLeft (knobsArea, gap);
                knob5.setBounds (knobsArea.reduced (inset));
            }
        };

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

        layoutFiveKnobCard (compressorCard,
                            compressorInputGainLabel,
                            compressorThresholdLabel,
                            compressorRatioLabel,
                            compressorAttackLabel,
                            compressorReleaseLabel,
                            compressorInputGainSlider,
                            compressorThresholdSlider,
                            compressorRatioSlider,
                            compressorAttackSlider,
                            compressorReleaseSlider,
                            compressorStateLabel,
                            compressorToggle);

        layoutFourKnobCard (reverbCard,
                            reverbRoomSizeLabel,
                            reverbDryWetLabel,
                            reverbDecayLabel,
                            reverbWidthLabel,
                            reverbRoomSizeSlider,
                            reverbDryWetSlider,
                            reverbDecaySlider,
                            reverbWidthSlider,
                            reverbStateLabel,
                            reverbToggle);

        layoutThreeKnobCard (toneStackCard,
                             toneStackBassLabel,
                             toneStackMidLabel,
                             toneStackTrebleLabel,
                             toneStackBassSlider,
                             toneStackMidSlider,
                             toneStackTrebleSlider,
                             toneStackStateLabel,
                             toneStackToggle);
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

        const auto compOn = compressorEnabled.load (std::memory_order_relaxed);
        const auto compThresh = compressorThresholdDb.load (std::memory_order_relaxed);
        const auto compRatio = compressorRatio.load (std::memory_order_relaxed);

        const auto reverbOn = reverbEnabled.load (std::memory_order_relaxed);
        const auto reverbMix = reverbDryWetMix.load (std::memory_order_relaxed);

        const auto toneOn = toneStackEnabled.load (std::memory_order_relaxed);
        const auto toneBass = toneStackBassDb.load (std::memory_order_relaxed);
        const auto toneMid = toneStackMidDb.load (std::memory_order_relaxed);
        const auto toneTreble = toneStackTrebleDb.load (std::memory_order_relaxed);

        juce::String chain = bypassed ? "Bypassed"
                                      : "Clean Boost " + juce::String (boostOn ? "ON" : "OFF")
                                            + " | Gain " + juce::String (boostDb, 1) + " dB"
                                            + " -> Overdrive " + juce::String (odOn ? "ON" : "OFF")
                                            + " | Drive " + juce::String (odDrive, 0) + "%"
                                            + " | Level " + juce::String (odLevel, 0) + "%"
                                            + " -> EQ " + juce::String (eqOn ? "ON" : "OFF")
                                            + " | Bass " + juce::String (eqBass, 1) + " dB"
                                            + " | Mid " + juce::String (eqMid, 1) + " dB"
                                            + " | Treble " + juce::String (eqTreble, 1) + " dB"
                                            + " -> Compressor " + juce::String (compOn ? "ON" : "OFF")
                                            + " | Thresh " + juce::String (compThresh, 1) + " dB"
                                            + " | Ratio " + juce::String (compRatio, 1) + ":1"
                                            + " -> Reverb " + juce::String (reverbOn ? "ON" : "OFF")
                                            + " | Mix " + juce::String (reverbMix, 2)
                                            + " -> Tone Stack " + juce::String (toneOn ? "ON" : "OFF")
                                            + " | B:" + juce::String (toneBass, 1)
                                            + " M:" + juce::String (toneMid, 1)
                                            + " T:" + juce::String (toneTreble, 1) + " dB";

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

    monitorUI.setInputLevels (rmsDb, peakHoldDb, clipped);

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

    monitorUI.setOutputLevels (outRmsDb, outputPeakHoldDb, outClipped);

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
