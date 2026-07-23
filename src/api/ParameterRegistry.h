#pragma once

#include <JuceHeader.h>

#include <functional>
#include <string>
#include <vector>

namespace milodikfx::api
{
/**
 * Describes one automatable value: how to show it, what range it accepts, and
 * how to read and write it on the live processor.
 */
struct ParameterDescriptor
{
    std::string id;
    std::string label;
    std::string unit;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float step = 0.01f;
    float defaultValue = 0.0f;

    /** Rendered as a switch rather than a knob; stored as 0 or 1. */
    bool isBoolean = false;

    std::function<float()> get;
    std::function<void (float)> set;

    /**
     * Text-valued parameters: a file choice rather than a number.
     *
     * Set getText/setText instead of get/set. getOptions supplies the choices
     * the UI offers, evaluated on read so a newly dropped file appears without
     * anything having to invalidate a cache.
     */
    bool isText = false;
    std::function<juce::String()> getText;
    std::function<void (const juce::String&)> setText;
    std::function<juce::StringArray()> getOptions;
};

/** One effect block: an enable switch plus its parameters, in chain order. */
struct EffectDescriptor
{
    std::string id;
    std::string label;
    std::string description;

    std::function<bool()> isEnabled;
    std::function<void (bool)> setEnabled;

    std::vector<ParameterDescriptor> parameters;
};

/**
 * The single source of truth for what the UI, the REST layer and the settings
 * file all agree the parameter set is.
 *
 * Adding a control used to mean editing six files that could silently disagree.
 * Registering it here instead makes it appear in /api/effects, in the UI, and in
 * the persisted settings at once.
 */
class ParameterRegistry
{
public:
    void addEffect (EffectDescriptor effect);

    const std::vector<EffectDescriptor>& getEffects() const noexcept { return effects; }

    const EffectDescriptor* findEffect (const std::string& effectId) const noexcept;
    const ParameterDescriptor* findParameter (const std::string& effectId, const std::string& parameterId) const noexcept;

    /** Applies a value, clamped to the descriptor's range. Returns the stored value. */
    bool setParameter (const std::string& effectId, const std::string& parameterId, float value, float& outApplied) const;

    /** Applies a text value. Fails for numeric parameters. */
    bool setTextParameter (const std::string& effectId,
                           const std::string& parameterId,
                           const juce::String& value,
                           juce::String& outApplied) const;

    bool setEffectEnabled (const std::string& effectId, bool enabled) const;

    /** Full state as a juce::var, so juce::JSON handles all the escaping. */
    juce::var toVar() const;
    juce::var effectToVar (const EffectDescriptor& effect) const;

    /**
     * Compact snapshot for presets and settings:
     * { "<effectId>": { "enabled": bool, "params": { "<paramId>": number } } }
     */
    juce::var captureState() const;

    /** Applies a captured snapshot. Unknown ids are ignored. Returns values applied. */
    int applyState (const juce::var& state) const;

    /** Every registered value as "<effect>.<parameter>" -> current value. */
    void forEachParameter (const std::function<void (const EffectDescriptor&, const ParameterDescriptor&)>& fn) const;

    /** Invoked after any successful mutation so settings can be marked dirty. */
    std::function<void()> onChanged;

    /**
     * Optional override for what `captureState` records for a numeric parameter.
     * When a modifier owns a parameter its live value is a swept sample; this
     * lets the app substitute the base value the parameter will return to, so a
     * preset saved mid-sweep stores something sensible. Returns true and writes
     * `out` when it has an override for that parameter.
     */
    std::function<bool (const std::string&, const std::string&, float&)> baseValueProvider;

private:
    void notifyChanged() const;

    std::vector<EffectDescriptor> effects;
};
} // namespace milodikfx::api
