#include "EffectCardComponent.h"

namespace milodikfx::ui {

EffectCardComponent::EffectCardComponent (const Config& cfg)
    : config (cfg)
{
}

void EffectCardComponent::setTitle (const juce::String& newTitle)
{
    config.title = newTitle;
    repaint();
}

void EffectCardComponent::setAccentColour (juce::Colour newAccent)
{
    config.accentColour = newAccent;
    repaint();
}

void EffectCardComponent::setEnabledState (bool isEnabled)
{
    config.isEnabled = isEnabled;
    repaint();
}

juce::Rectangle<int> EffectCardComponent::getContentBounds() const
{
    auto b = getLocalBounds().reduced (14);
    b.removeFromTop (38);    // Header height
    b.removeFromTop (10);    // Gap after header
    b.removeFromBottom (10); // Bottom padding
    return b;
}

void EffectCardComponent::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const auto radius = static_cast<float> (config.cornerRadius);

    // Body with gradient
    {
        juce::ColourGradient bodyGrad (juce::Colour (0xff232629), b.getCentreX(), b.getY(),
                                       juce::Colour (0xff121416), b.getCentreX(), b.getBottom(), false);
        g.setGradientFill (bodyGrad);
        g.fillRoundedRectangle (b, radius);

        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.drawRoundedRectangle (b, radius, 1.5f);
    }

    // Header with accent gradient
    {
        auto header = b;
        header.setHeight (38.0f);

        const auto headerAccent = config.isEnabled ? config.accentColour.withAlpha (0.55f)
                                                   : config.accentColour.withAlpha (0.14f);

        juce::ColourGradient headerGrad (headerAccent, header.getX(), header.getY(),
                                        juce::Colour (0xff0f1113), header.getX(), header.getBottom(), false);
        g.setGradientFill (headerGrad);
        g.fillRoundedRectangle (header, radius);

        // Square off the bottom of the header for a nicer card look
        g.setColour (juce::Colour (0xff0f1113).withAlpha (0.8f));
        g.fillRect (juce::Rectangle<float> (header.getX(), header.getBottom() - radius, header.getWidth(), radius));

        // LED indicator
        const auto led = config.isEnabled ? config.accentColour.withAlpha (0.95f)
                                          : juce::Colours::black.withAlpha (0.35f);
        g.setColour (led);
        g.fillEllipse (juce::Rectangle<float> (header.getX() + 12.0f, header.getCentreY() - 6.0f, 12.0f, 12.0f));
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawEllipse (juce::Rectangle<float> (header.getX() + 12.0f, header.getCentreY() - 6.0f, 12.0f, 12.0f), 1.0f);

        // Title text
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
        g.drawFittedText (config.title, header.toNearestInt().reduced (24, 0), juce::Justification::centred, 1);
    }
}

} // namespace milodikfx::ui
