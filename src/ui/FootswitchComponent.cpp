#include "FootswitchComponent.h"

FootswitchComponent::FootswitchComponent (const juce::String& effectName, const Config& configParam)
    : config (configParam),
      button (effectName, juce::DrawableButton::ButtonStyle::ImageFitted)
{
    stateLabel.setText ("OFF", juce::dontSendNotification);
    stateLabel.setJustificationType (juce::Justification::centred);
    stateLabel.setFont (juce::Font (10.0f));
    stateLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    addAndMakeVisible (stateLabel);

    button.setClickingTogglesState (true);
    button.setColour (juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    button.onClick = [this]
    {
        toggleState = button.getToggleState();
        stateLabel.setText (toggleState ? "ON" : "OFF", juce::dontSendNotification);
        if (onToggleCallback)
            onToggleCallback (toggleState);
    };
    addAndMakeVisible (button);

    updateVisuals();
}

void FootswitchComponent::resized()
{
    auto bounds = getLocalBounds();

    const auto labelH = 16;
    const auto gap = 4;

    stateLabel.setBounds (bounds.removeFromTop (labelH));
    bounds.removeFromTop (gap);

    const auto buttonSize = config.buttonSize;
    button.setBounds (bounds.withSizeKeepingCentre (buttonSize, buttonSize));
}

void FootswitchComponent::setToggleState (bool newState, juce::NotificationType notification)
{
    toggleState = newState;
    button.setToggleState (newState, notification);
    stateLabel.setText (newState ? "ON" : "OFF", juce::dontSendNotification);
}

void FootswitchComponent::updateVisuals()
{
    if (toggleState)
        stateLabel.setColour (juce::Label::textColourId, config.accentColour);
    else
        stateLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
}
