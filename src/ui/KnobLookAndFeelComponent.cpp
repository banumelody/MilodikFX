#include "KnobLookAndFeelComponent.h"

namespace milodikfx::ui {

KnobLookAndFeelComponent::KnobLookAndFeelComponent()
{
    setColourScheme (juce::LookAndFeel_V4::getDarkColourScheme());
}

void KnobLookAndFeelComponent::drawRotarySlider (juce::Graphics& g,
                                                 int x,
                                                 int y,
                                                 int width,
                                                 int height,
                                                 float sliderPosProportional,
                                                 float rotaryStartAngle,
                                                 float rotaryEndAngle,
                                                 juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (6.0f);
    const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());

    const auto radius = (size * 0.5f) - 2.0f;
    const auto cx = bounds.getCentreX();
    const auto cy = bounds.getCentreY();

    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    const auto fill = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const auto outline = slider.findColour (juce::Slider::rotarySliderOutlineColourId);

    const auto knobArea = juce::Rectangle<float> (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    // Knob body with gradient
    {
        juce::ColourGradient grad (juce::Colour (0xff2c2f33), cx, cy - radius,
                                   juce::Colour (0xff0f1113), cx, cy + radius, false);
        g.setGradientFill (grad);
        g.fillEllipse (knobArea);

        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawEllipse (knobArea, 1.5f);

        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (knobArea.reduced (2.0f), 1.0f);
    }

    // Arc ring (background + value)
    {
        const auto arcRadius = radius + 3.0f;
        const auto arcThickness = 4.0f;

        // Background arc
        juce::Path bgArc;
        bgArc.addCentredArc (cx, cy, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (outline.withAlpha (0.35f));
        g.strokePath (bgArc, juce::PathStrokeType (arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Value arc
        juce::Path valueArc;
        valueArc.addCentredArc (cx, cy, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
        g.setColour (fill);
        g.strokePath (valueArc, juce::PathStrokeType (arcThickness + 0.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Pointer indicator
    {
        const auto pointerLen = radius * 0.75f;
        const auto pointerThickness = 2.0f;

        const auto dx = std::cos (angle - juce::MathConstants<float>::halfPi);
        const auto dy = std::sin (angle - juce::MathConstants<float>::halfPi);

        const auto x2 = cx + pointerLen * dx;
        const auto y2 = cy + pointerLen * dy;

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawLine (cx, cy, x2, y2, pointerThickness);

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (juce::Rectangle<float> (cx - 3.5f, cy - 3.5f, 7.0f, 7.0f));
    }
}

} // namespace milodikfx::ui
