#pragma once

#include <JuceHeader.h>

/**
 * Modular knob component with consistent styling.
 * Wraps juce::Slider for easy customization and reusability.
 */
class KnobComponent : public juce::Component
{
public:
    struct Config
    {
        juce::Colour accentColour = juce::Colour (0xff3aa3ff);
        juce::Colour textBoxBg = juce::Colour (0xff0f1113);
        juce::Colour textBoxOutline = juce::Colours::black.withAlpha (0.65f);
        juce::Colour textBoxText = juce::Colours::white.withAlpha (0.9f);
        float minValue = -12.0f;
        float maxValue = 12.0f;
        float interval = 0.1f;
        juce::String suffix = " dB";
        int decimalPlaces = 1;
        bool enableDoubleClickReset = true;
        double resetValue = 0.0;
        bool enablePopup = true;
    };

    explicit KnobComponent (const juce::String& labelText, const Config& config = {});
    ~KnobComponent() override = default;

    void resized() override;

    juce::Slider& getSlider() { return slider; }
    const juce::Slider& getSlider() const { return slider; }

    juce::Label& getLabel() { return label; }
    const juce::Label& getLabel() const { return label; }

    void setValue (double newValue, juce::NotificationType notification = juce::sendNotificationAsync)
    {
        slider.setValue (newValue, notification);
    }

    double getValue() const { return slider.getValue(); }

    void setOnValueChange (std::function<void()> callback)
    {
        slider.onValueChange = std::move (callback);
    }

private:
    juce::Label label;
    juce::Slider slider;
    Config config;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KnobComponent)
};
