#pragma once

#include <JuceHeader.h>

namespace milodikfx::ui {

/**
 * @brief Preset management UI: combo box + Save/Load/Delete buttons.
 *
 * Displays:
 * - Preset name label
 * - Preset selector combo box
 * - Save button
 * - Load button
 * - Delete button
 *
 * Encapsulates preset UI in one self-contained component.
 */
class PresetManagerUIComponent final : public juce::Component
{
public:
    struct Config
    {
        juce::Colour labelColour { juce::Colours::white.withAlpha (0.9f) };
        juce::Colour buttonColour { juce::Colours::limegreen };
        int labelHeight { 18 };
        int buttonHeight { 28 };
        int gapSize { 6 };
        int minButtonWidth { 60 };
    };

    explicit PresetManagerUIComponent (const Config& config = {});
    ~PresetManagerUIComponent() override = default;

    /**
     * @brief Get currently selected preset name.
     */
    juce::String getSelectedPresetName() const { return presetCombo.getItemText (presetCombo.getSelectedItemIndex()); }

    /**
     * @brief Set available presets in combo box.
     */
    void setPresetsList (const juce::StringArray& presets, int selectedIndex = 0);

    /**
     * @brief Get selected preset index.
     */
    int getSelectedPresetIndex() const { return presetCombo.getSelectedItemIndex(); }

    /**
     * @brief Set selected preset by index.
     */
    void setSelectedPreset (int index);

    /**
     * @brief Callbacks for button presses.
     */
    std::function<void()> onSavePressed;
    std::function<void()> onLoadPressed;
    std::function<void()> onDeletePressed;
    std::function<void(int)> onPresetSelected;

    void resized() override;

private:
    Config config;
    juce::Label presetLabel { "Preset" };
    juce::ComboBox presetCombo;
    juce::TextButton saveButton { "Save" };
    juce::TextButton loadButton { "Load" };
    juce::TextButton deleteButton { "Delete" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManagerUIComponent)
};

} // namespace milodikfx::ui
