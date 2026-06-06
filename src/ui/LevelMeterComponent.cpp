#include "LevelMeterComponent.h"

namespace milodikfx::ui {

LevelMeterComponent::LevelMeterComponent (const Config& cfg)
    : config (cfg)
{
}

void LevelMeterComponent::setLevelsDb (float newRmsDb, float newPeakHoldDb, bool isClippedNow)
{
    rmsDb = juce::jlimit (-100.0f, 0.0f, newRmsDb);
    peakHoldDb = juce::jlimit (-100.0f, 0.0f, newPeakHoldDb);
    isClipped = isClippedNow;
    repaint();
}

void LevelMeterComponent::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // Background
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (b, static_cast<float> (config.cornerRadius));

    // RMS bar (vertical fill from bottom)
    {
        const auto rmsNorm = juce::jmap (rmsDb, -60.0f, 0.0f, 0.0f, 1.0f);
        const auto rmsClamped = juce::jlimit (0.0f, 1.0f, rmsNorm);

        auto fill = b;
        fill.removeFromTop (b.getHeight() * (1.0f - rmsClamped));

        const auto rmsColour = rmsDb > config.peakThresholdDb ? config.rmsWarningColour : config.rmsGoodColour;
        g.setColour (rmsColour);
        g.fillRoundedRectangle (fill.reduced (3.0f), static_cast<float> (config.cornerRadius));
    }

    // Peak-hold marker (horizontal line)
    {
        const auto peakNorm = juce::jmap (peakHoldDb, -60.0f, 0.0f, 0.0f, 1.0f);
        const auto peakClamped = juce::jlimit (0.0f, 1.0f, peakNorm);
        const auto y = b.getBottom() - (b.getHeight() * peakClamped);

        g.setColour (config.peakMarkerColour);
        g.drawLine (b.getX() + 3.0f, y, b.getRight() - 3.0f, y, 2.0f);
    }

    // Border (semi-transparent white)
    {
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawRoundedRectangle (b, static_cast<float> (config.cornerRadius), 1.0f);
    }

    // Clipping indicator (red border)
    if (isClipped)
    {
        g.setColour (config.clippedColour.withAlpha (0.9f));
        g.drawRoundedRectangle (b.reduced (1.0f), static_cast<float> (config.cornerRadius), 2.0f);
    }
}

} // namespace milodikfx::ui
