#pragma once

#include <JuceHeader.h>

namespace milodikfx::ui {

/**
 * @brief Reusable effect card component with customizable header and content area.
 *
 * Displays a rounded card with:
 * - Custom title in header
 * - Accent color for enabled/disabled state
 * - LED indicator
 * - Content bounds available for child controls
 */
class EffectCardComponent final : public juce::Component
{
public:
    struct Config
    {
        juce::String title { "Effect" };
        juce::Colour accentColour { juce::Colours::white };
        bool isEnabled { true };
        int cornerRadius { 10 };
    };

    explicit EffectCardComponent (const Config& config = {});
    ~EffectCardComponent() override = default;

    void setTitle (const juce::String& newTitle);
    void setAccentColour (juce::Colour newAccent);
    void setEnabledState (bool isEnabled);

    /**
     * @brief Get the content area bounds (excluding header and padding).
     * Useful for layout calculations in parent component's resized().
     */
    juce::Rectangle<int> getContentBounds() const;

    void paint (juce::Graphics& g) override;

private:
    Config config;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectCardComponent)
};

} // namespace milodikfx::ui
