#include "MonitorRowComponent.h"

namespace milodikfx::ui {

MonitorRowComponent::MonitorRowComponent (const Config& cfg)
    : config (cfg), meters (MeterRowComponent::Config { "Input", "Output" })
{
    // Monitor toggle
    monitorLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (monitorLabel);
    addAndMakeVisible (monitorToggle);
    monitorToggle.onClick = [this]
    {
        if (onMonitorToggle)
            onMonitorToggle (monitorToggle.getToggleState());
    };

    // Mute toggle
    muteLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (muteLabel);
    addAndMakeVisible (muteToggle);
    muteToggle.onClick = [this]
    {
        if (onMuteToggle)
            onMuteToggle (muteToggle.getToggleState());
    };

    // Routing combo
    routingLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (routingLabel);
    routingCombo.addItem ("Dry", 1);
    routingCombo.addItem ("Wet", 2);
    routingCombo.addItem ("Parallel", 3);
    routingCombo.onChange = [this]
    {
        if (onRoutingChange)
            onRoutingChange (routingCombo.getSelectedId());
    };
    addAndMakeVisible (routingCombo);

    // Gain slider
    gainLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (gainLabel);
    gainSlider.setRange (config.minGainDb, config.maxGainDb, config.gainInterval);
    gainSlider.setValue (0.0);
    gainSlider.setTextValueSuffix (" dB");
    gainSlider.setColour (juce::Slider::thumbColourId, config.sliderFillColour);
    gainSlider.onValueChange = [this]
    {
        if (onGainChange)
            onGainChange (static_cast<float> (gainSlider.getValue()));
    };
    addAndMakeVisible (gainSlider);

    // Meters
    addAndMakeVisible (meters);
}

void MonitorRowComponent::setMonitorEnabled (bool enabled)
{
    monitorToggle.setToggleState (enabled, juce::dontSendNotification);
}

void MonitorRowComponent::setMuted (bool muted)
{
    muteToggle.setToggleState (muted, juce::dontSendNotification);
}

void MonitorRowComponent::setGainDb (float db)
{
    gainSlider.setValue (db, juce::dontSendNotification);
}

void MonitorRowComponent::setRoutingMode (int modeId)
{
    routingCombo.setSelectedId (modeId, juce::dontSendNotification);
}

void MonitorRowComponent::setInputLevels (float rmsDb, float peakHoldDb, bool isClipped)
{
    meters.setLeftLevelsDb (rmsDb, peakHoldDb, isClipped);
}

void MonitorRowComponent::setOutputLevels (float rmsDb, float peakHoldDb, bool isClipped)
{
    meters.setRightLevelsDb (rmsDb, peakHoldDb, isClipped);
}

void MonitorRowComponent::resized()
{
    auto b = getLocalBounds();

    // Controls row (monitor, mute, routing, gain)
    auto controlsRow = b.removeFromTop (config.controlsHeight);

    // Monitor
    auto monitorArea = controlsRow.removeFromLeft (controlsRow.getWidth() / 4);
    monitorLabel.setBounds (monitorArea.removeFromTop (12));
    monitorToggle.setBounds (monitorArea);

    controlsRow.removeFromLeft (config.gapSize);

    // Mute
    auto muteArea = controlsRow.removeFromLeft (controlsRow.getWidth() / 3);
    muteLabel.setBounds (muteArea.removeFromTop (12));
    muteToggle.setBounds (muteArea);

    controlsRow.removeFromLeft (config.gapSize);

    // Routing
    auto routingArea = controlsRow.removeFromLeft (controlsRow.getWidth() / 2);
    routingLabel.setBounds (routingArea.removeFromTop (12));
    routingCombo.setBounds (routingArea);

    controlsRow.removeFromLeft (config.gapSize);

    // Gain
    gainLabel.setBounds (controlsRow.removeFromTop (12));
    gainSlider.setBounds (controlsRow);

    b.removeFromTop (config.gapSize);

    // Meters
    auto metersArea = b.removeFromTop (config.meterHeight);
    meters.setBounds (metersArea);
}

} // namespace milodikfx::ui
