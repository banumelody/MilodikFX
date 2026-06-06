#pragma once

#include <JuceHeader.h>

/**
 * Standardized footswitch component with consistent sizing and styling.
 * Combines a state label ("ON"/"OFF") and a toggle button.
 */
class FootswitchComponent : public juce::Component
{
public:
    struct Config
    {
        juce::Colour accentColour = juce::Colour (0xff3aa3ff);
        int buttonSize = 56;  // Fixed size for consistency
        bool showLabel = true;
    };

    explicit FootswitchComponent (const juce::String& effectName, const Config& config = {});
    ~FootswitchComponent() override = default;

    void resized() override;

    bool getToggleState() const { return toggleState; }
    void setToggleState (bool newState, juce::NotificationType notification = juce::sendNotificationAsync);

    void setOnToggle (std::function<void(bool)> callback)
    {
        onToggleCallback = std::move (callback);
    }

    void setAccentColour (juce::Colour newColour)
    {
        config.accentColour = newColour;
        updateVisuals();
    }

private:
    juce::Label stateLabel;
    juce::DrawableButton button;
    Config config;
    bool toggleState = false;
    std::function<void(bool)> onToggleCallback;

    void updateVisuals();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FootswitchComponent)
};
