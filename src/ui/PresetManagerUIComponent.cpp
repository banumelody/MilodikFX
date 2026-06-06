#include "PresetManagerUIComponent.h"

namespace milodikfx::ui {

PresetManagerUIComponent::PresetManagerUIComponent (const Config& cfg)
    : config (cfg)
{
    presetLabel.setJustificationType (juce::Justification::centred);
    presetLabel.setColour (juce::Label::textColourId, config.labelColour);
    addAndMakeVisible (presetLabel);

    presetCombo.onChange = [this]
    {
        if (onPresetSelected)
            onPresetSelected (presetCombo.getSelectedItemIndex());
    };
    addAndMakeVisible (presetCombo);

    saveButton.onClick = [this]
    {
        if (onSavePressed)
            onSavePressed();
    };
    saveButton.setColour (juce::TextButton::buttonColourId, config.buttonColour);
    addAndMakeVisible (saveButton);

    loadButton.onClick = [this]
    {
        if (onLoadPressed)
            onLoadPressed();
    };
    loadButton.setColour (juce::TextButton::buttonColourId, config.buttonColour);
    addAndMakeVisible (loadButton);

    deleteButton.onClick = [this]
    {
        if (onDeletePressed)
            onDeletePressed();
    };
    deleteButton.setColour (juce::TextButton::buttonColourId, juce::Colours::red);
    addAndMakeVisible (deleteButton);
}

void PresetManagerUIComponent::setPresetsList (const juce::StringArray& presets, int selectedIndex)
{
    presetCombo.clear();
    for (int i = 0; i < presets.size(); ++i)
        presetCombo.addItem (presets[i], i + 1);

    if (selectedIndex >= 0 && selectedIndex < presets.size())
        presetCombo.setSelectedItemIndex (selectedIndex, juce::dontSendNotification);
}

void PresetManagerUIComponent::setSelectedPreset (int index)
{
    presetCombo.setSelectedItemIndex (index, juce::dontSendNotification);
}

void PresetManagerUIComponent::resized()
{
    auto b = getLocalBounds();

    // Label
    presetLabel.setBounds (b.removeFromTop (config.labelHeight));
    b.removeFromTop (config.gapSize);

    // Controls row (combo + buttons)
    auto controlsRow = b.removeFromTop (config.buttonHeight);

    // Preset combo takes most space
    const auto comboWidth = controlsRow.getWidth() - (config.minButtonWidth * 3 + config.gapSize * 3);
    presetCombo.setBounds (controlsRow.removeFromLeft (comboWidth));
    controlsRow.removeFromLeft (config.gapSize);

    // Buttons
    saveButton.setBounds (controlsRow.removeFromLeft (config.minButtonWidth));
    controlsRow.removeFromLeft (config.gapSize);

    loadButton.setBounds (controlsRow.removeFromLeft (config.minButtonWidth));
    controlsRow.removeFromLeft (config.gapSize);

    deleteButton.setBounds (controlsRow.removeFromLeft (config.minButtonWidth));
}

} // namespace milodikfx::ui
