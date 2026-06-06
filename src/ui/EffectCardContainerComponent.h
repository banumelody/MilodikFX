#pragma once

#include <JuceHeader.h>
#include "EffectCardComponent.h"
#include "KnobComponent.h"
#include "FootswitchComponent.h"

namespace milodikfx::ui {

/**
 * @brief Container for effect card with automatic layout for knobs + footswitch.
 *
 * Handles three layout patterns:
 * - 1-knob: Knob + footswitch vertically stacked
 * - 2-knob: Knobs horizontal or vertical (adaptive based on width)
 * - 3-knob: EQ layout - 3 knobs horizontal with footswitch below
 *
 * Manages content bounds and delegates layout to child components.
 */
class EffectCardContainerComponent final : public juce::Component
{
public:
    enum class LayoutMode
    {
        OneKnob,
        TwoKnob,
        ThreeKnob
    };

    struct Config
    {
        LayoutMode layoutMode { LayoutMode::OneKnob };
        juce::String cardTitle { "Effect" };
        juce::Colour cardAccent { juce::Colours::white };
        int minCardWidth { 140 };
        int maxCardWidth { 200 };
        int maxCardHeight { 240 };
    };

    explicit EffectCardContainerComponent (const Config& config = {});
    ~EffectCardContainerComponent() override = default;

    EffectCardComponent& getCard() { return card; }
    const EffectCardComponent& getCard() const { return card; }

    void setCardTitle (const juce::String& title);
    void setCardAccent (juce::Colour accent);
    void setCardEnabledState (bool isEnabled);

    void resized() override;

private:
    Config config;
    EffectCardComponent card;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectCardContainerComponent)
};

} // namespace milodikfx::ui
