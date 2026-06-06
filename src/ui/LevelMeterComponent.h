#pragma once

#include <JuceHeader.h>

namespace milodikfx::ui {

/**
 * @brief Reusable level meter component with RMS + peak hold visualization.
 *
 * Displays:
 * - RMS level as vertical bar (green/orange based on threshold)
 * - Peak hold marker (yellow horizontal line)
 * - Clipping indicator (red border)
 */
class LevelMeterComponent final : public juce::Component
{
public:
    struct Config
    {
        float peakThresholdDb { -6.0f };     // Above this = orange, below = green
        juce::Colour rmsGoodColour { juce::Colours::limegreen };
        juce::Colour rmsWarningColour { juce::Colours::orange };
        juce::Colour peakMarkerColour { juce::Colours::yellow };
        juce::Colour clippedColour { juce::Colours::red };
        int cornerRadius { 4 };
    };

    explicit LevelMeterComponent (const Config& config = {});
    ~LevelMeterComponent() override = default;

    /**
     * @brief Update meter levels.
     * @param rmsDb RMS level in dB (clamped to -100..0)
     * @param peakHoldDb Peak hold level in dB (clamped to -100..0)
     * @param isClipped Whether clipping is active
     */
    void setLevelsDb (float rmsDb, float peakHoldDb, bool isClipped);

    void paint (juce::Graphics& g) override;

private:
    Config config;
    float rmsDb { -100.0f };
    float peakHoldDb { -100.0f };
    bool isClipped { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeterComponent)
};

} // namespace milodikfx::ui
