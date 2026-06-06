#pragma once

#include <JuceHeader.h>
#include "MeterRowComponent.h"

namespace milodikfx::ui {

/**
 * @brief Monitor controls row: toggle + routing + gain + metering.
 *
 * Displays:
 * - Monitor enable toggle + Mute toggle
 * - Routing mode selector
 * - Monitor gain slider
 * - Input/Output meter pair
 *
 * Encapsulates all monitoring UI in one self-contained component.
 */
class MonitorRowComponent final : public juce::Component
{
public:
    struct Config
    {
        juce::Colour toggleOnColour { juce::Colours::limegreen };
        juce::Colour toggleOffColour { juce::Colours::white.withAlpha (0.5f) };
        juce::Colour sliderFillColour { juce::Colours::limegreen };
        float minGainDb { -48.0f };
        float maxGainDb { 12.0f };
        float gainInterval { 0.1f };
        int meterHeight { 60 };
        int controlsHeight { 28 };
        int gapSize { 8 };
    };

    explicit MonitorRowComponent (const Config& config = {});
    ~MonitorRowComponent() override = default;

    /**
     * @brief Get/set monitor enabled state.
     */
    bool isMonitorEnabled() const { return monitorToggle.getToggleState(); }
    void setMonitorEnabled (bool enabled);

    /**
     * @brief Get/set mute state.
     */
    bool isMuted() const { return muteToggle.getToggleState(); }
    void setMuted (bool muted);

    /**
     * @brief Get/set monitor gain (dB).
     */
    float getGainDb() const { return static_cast<float> (gainSlider.getValue()); }
    void setGainDb (float db);

    /**
     * @brief Get routing mode (1-indexed for JUCE ComboBox).
     */
    int getRoutingMode() const { return routingCombo.getSelectedId(); }
    void setRoutingMode (int modeId);

    /**
     * @brief Update meter levels.
     */
    void setInputLevels (float rmsDb, float peakHoldDb, bool isClipped);
    void setOutputLevels (float rmsDb, float peakHoldDb, bool isClipped);

    /**
     * @brief Callbacks for state changes.
     */
    std::function<void(bool)> onMonitorToggle;
    std::function<void(bool)> onMuteToggle;
    std::function<void(float)> onGainChange;
    std::function<void(int)> onRoutingChange;

    void resized() override;

private:
    Config config;
    juce::Label monitorLabel { "Monitor" };
    juce::ToggleButton monitorToggle;
    juce::Label muteLabel { "Mute" };
    juce::ToggleButton muteToggle;

    juce::Label routingLabel { "Routing" };
    juce::ComboBox routingCombo;

    juce::Label gainLabel { "Gain" };
    juce::Slider gainSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    MeterRowComponent meters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonitorRowComponent)
};

} // namespace milodikfx::ui
