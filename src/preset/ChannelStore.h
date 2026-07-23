#pragma once

#include <JuceHeader.h>

#include <map>
#include <string>
#include <vector>

#include "api/ParameterRegistry.h"

namespace milodikfx::preset
{
/**
 * Four saved sounds per effect -- channels A, B, C, D -- the way a Fractal FM9
 * block holds four. One drive block can keep a low-gain Tube Screamer in A and a
 * full-gain one in B; switching channel jumps the whole block to that saved
 * state without a second block in the chain.
 *
 * This deliberately extends, rather than contradicts, the "a scene stores only
 * enable flags" rule. The worry there was jumping a parameter to a value you
 * cannot see, on a control you were not touching. A channel jumps only to a
 * *named state you built yourself*, shown plainly as a tab on the card, and a
 * scene still stores no free parameter values -- only which channel index to
 * select. What is invisible stays absent.
 *
 * Switching channel first saves the live values back into the outgoing channel,
 * so edits made while on A are kept when you move to B -- the block feels live,
 * like the hardware, without intercepting every knob turn. Applying a channel is
 * a run of ordinary registry writes (atomics, smoothed), the same discipline a
 * MIDI CC already writes a parameter with, so it is realtime-safe and never
 * touches reset(): a delay or reverb tail rings on across a channel change.
 */
class ChannelStore final
{
public:
    static constexpr int kNumChannels = 4;

    explicit ChannelStore (milodikfx::api::ParameterRegistry& registryToUse);

    /**
     * Selects a channel for one effect: saves the live values into the channel
     * that was active, then applies the chosen one. Returns false for an unknown
     * effect or a bad index.
     */
    bool recall (const std::string& effectId, int index);

    /** Overwrites a channel with the effect's current live values. */
    bool save (const std::string& effectId, int index);

    /** Renames a channel. Empty names are refused. */
    bool setName (const std::string& effectId, int index, const juce::String& name);

    int getActive (const std::string& effectId) const;
    juce::String getName (const std::string& effectId, int index) const;

    /** Serialised into the preset and the settings file. */
    juce::var toVar() const;
    void fromVar (const juce::var& value);

    /** Every effect gets four channels, all holding the chain as it is now. */
    void resetToCurrent();

    /**
     * Saves each effect's live values into its active channel. Call before
     * capturing a preset so the stored active channel matches what is playing --
     * live edits go to the sound but not into the channel until a switch, so
     * without this a save-then-reload would revert the active channel's edits.
     */
    void commitActive();

    static bool isValidIndex (int index) noexcept { return index >= 0 && index < kNumChannels; }

    /** Invoked after a mutation so settings can be marked dirty. */
    std::function<void()> onChanged;

    /** Optional: supplies a parameter's base value when a modifier owns it, so a
        channel does not capture a swept sample. Same shape as the registry's. */
    void setBaseValueProvider (std::function<bool (const std::string&, const std::string&, float&)> provider)
    {
        baseValueProvider = std::move (provider);
    }

private:
    struct Channel
    {
        juce::String name;
        std::map<std::string, float> params;
        std::map<std::string, juce::String> textParams;
        bool populated = false;
    };

    struct EffectChannels
    {
        std::array<Channel, kNumChannels> channels;
        int active = 0;
    };

    Channel captureEffect (const milodikfx::api::EffectDescriptor& effect) const;
    void applyChannel (const milodikfx::api::EffectDescriptor& effect, const Channel& channel) const;
    EffectChannels& ensure (const milodikfx::api::EffectDescriptor& effect);

    void notifyChanged() const;

    milodikfx::api::ParameterRegistry& registry;
    std::function<bool (const std::string&, const std::string&, float&)> baseValueProvider;
    std::map<std::string, EffectChannels> byEffect;
};
} // namespace milodikfx::preset
