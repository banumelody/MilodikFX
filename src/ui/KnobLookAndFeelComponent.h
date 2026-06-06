#pragma once

#include <JuceHeader.h>

namespace milodikfx::ui {

/**
 * @brief Custom Look & Feel for rotary knobs with gradient + arc + pointer styling.
 *
 * Draws knobs with:
 * - Gradient sphere body
 * - Arc ring showing range and value
 * - Pointer indicator
 * - Professional appearance matching MilodikFX design
 */
class KnobLookAndFeelComponent final : public juce::LookAndFeel_V4
{
public:
    KnobLookAndFeelComponent();
    ~KnobLookAndFeelComponent() override = default;

    void drawRotarySlider (juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KnobLookAndFeelComponent)
};

} // namespace milodikfx::ui
