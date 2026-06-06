#pragma once

#include <JuceHeader.h>
#include "LevelMeterComponent.h"

namespace milodikfx::ui {

/**
 * @brief Container for a pair of level meters (input/output) with labels.
 *
 * Displays:
 * - Left label + left meter (e.g., "INPUT")
 * - Right label + right meter (e.g., "OUTPUT")
 * - Both meters update in sync via setLevelsDb()
 */
class MeterRowComponent final : public juce::Component
{
public:
    struct Config
    {
        juce::String leftLabel { "Input" };
        juce::String rightLabel { "Output" };
        juce::Colour labelColour { juce::Colours::white.withAlpha (0.9f) };
        int labelFontSize { 12 };
        int labelHeight { 16 };
        int meterGap { 12 };      // Horizontal gap between meters
        int verticalGap { 4 };    // Vertical gap between label and meter
    };

    explicit MeterRowComponent (const Config& config = {});
    ~MeterRowComponent() override = default;

    /**
     * @brief Update both meters with levels.
     */
    void setLeftLevelsDb (float rmsDb, float peakHoldDb, bool isClipped);
    void setRightLevelsDb (float rmsDb, float peakHoldDb, bool isClipped);

    void resized() override;

private:
    Config config;
    juce::Label leftLabel;
    juce::Label rightLabel;
    LevelMeterComponent leftMeter;
    LevelMeterComponent rightMeter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeterRowComponent)
};

} // namespace milodikfx::ui
