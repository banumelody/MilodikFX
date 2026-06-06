#include "EffectCardContainerComponent.h"

namespace milodikfx::ui {

EffectCardContainerComponent::EffectCardContainerComponent (const Config& cfg)
    : config (cfg), card (EffectCardComponent::Config { cfg.cardTitle, cfg.cardAccent })
{
    addAndMakeVisible (card);
}

void EffectCardContainerComponent::setCardTitle (const juce::String& title)
{
    card.setTitle (title);
}

void EffectCardContainerComponent::setCardAccent (juce::Colour accent)
{
    card.setAccentColour (accent);
}

void EffectCardContainerComponent::setCardEnabledState (bool isEnabled)
{
    card.setEnabledState (isEnabled);
}

void EffectCardContainerComponent::resized()
{
    card.setBounds (getLocalBounds());
}

} // namespace milodikfx::ui
