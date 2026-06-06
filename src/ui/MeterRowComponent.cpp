#include "MeterRowComponent.h"

namespace milodikfx::ui {

MeterRowComponent::MeterRowComponent (const Config& cfg)
    : config (cfg)
{
    leftLabel.setText (config.leftLabel, juce::dontSendNotification);
    leftLabel.setJustificationType (juce::Justification::centred);
    leftLabel.setColour (juce::Label::textColourId, config.labelColour);
    leftLabel.setFont (juce::Font (static_cast<float> (config.labelFontSize)));
    addAndMakeVisible (leftLabel);

    rightLabel.setText (config.rightLabel, juce::dontSendNotification);
    rightLabel.setJustificationType (juce::Justification::centred);
    rightLabel.setColour (juce::Label::textColourId, config.labelColour);
    rightLabel.setFont (juce::Font (static_cast<float> (config.labelFontSize)));
    addAndMakeVisible (rightLabel);

    addAndMakeVisible (leftMeter);
    addAndMakeVisible (rightMeter);
}

void MeterRowComponent::setLeftLevelsDb (float rmsDb, float peakHoldDb, bool isClipped)
{
    leftMeter.setLevelsDb (rmsDb, peakHoldDb, isClipped);
}

void MeterRowComponent::setRightLevelsDb (float rmsDb, float peakHoldDb, bool isClipped)
{
    rightMeter.setLevelsDb (rmsDb, peakHoldDb, isClipped);
}

void MeterRowComponent::resized()
{
    auto b = getLocalBounds();

    auto labelsRow = b.removeFromTop (config.labelHeight);
    const auto midX = labelsRow.getCentreX();
    auto leftLabelArea = labelsRow.removeFromLeft (midX);
    leftLabel.setBounds (leftLabelArea);
    rightLabel.setBounds (labelsRow);

    b.removeFromTop (config.verticalGap);

    const auto meterWidth = (b.getWidth() - config.meterGap) / 2;
    auto leftMeterArea = b.removeFromLeft (meterWidth);
    leftMeter.setBounds (leftMeterArea);

    b.removeFromLeft (config.meterGap);

    rightMeter.setBounds (b);
}

} // namespace milodikfx::ui
