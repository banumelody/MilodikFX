#include "KnobComponent.h"

KnobComponent::KnobComponent (const juce::String& labelText, const Config& configParam)
    : config (configParam)
{
    label.setText (labelText, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (11.0f));
    label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setColour (juce::Slider::rotarySliderFillColourId, config.accentColour);
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::white.withAlpha (0.22f));
    slider.setColour (juce::Slider::textBoxBackgroundColourId, config.textBoxBg);
    slider.setColour (juce::Slider::textBoxOutlineColourId, config.textBoxOutline);
    slider.setColour (juce::Slider::textBoxTextColourId, config.textBoxText);

    slider.setTextValueSuffix (config.suffix);
    slider.setNumDecimalPlacesToDisplay (config.decimalPlaces);
    slider.setRange (config.minValue, config.maxValue, config.interval);

    if (config.enableDoubleClickReset)
        slider.setDoubleClickReturnValue (true, config.resetValue);

    if (config.enablePopup)
        slider.setPopupDisplayEnabled (true, false, this);

    addAndMakeVisible (slider);
}

void KnobComponent::resized()
{
    auto bounds = getLocalBounds();
    const auto labelH = 14;
    const auto gap = 4;

    label.setBounds (bounds.removeFromTop (labelH));
    bounds.removeFromTop (gap);
    slider.setBounds (bounds);
}
