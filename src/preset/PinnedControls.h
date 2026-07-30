#pragma once

#include <JuceHeader.h>

#include <array>
#include <string>

#include "api/ParameterRegistry.h"

namespace milodikfx::preset
{
/**
 * The handful of knobs a preset wants within reach on stage.
 *
 * The Perform view exists so the screen is readable at arm's length, which means
 * it cannot show the whole rack -- but every sound has two or three controls you
 * really do reach for mid-set, and which ones they are is a property of the
 * sound, not of the app. A high-gain preset wants Drive and Gate; an ambient one
 * wants Delay Mix and Reverb Decay. So the pins live *in the preset*, and
 * changing preset changes what is on the big screen.
 *
 * Only a reference is stored -- which effect, which parameter -- never a value.
 * That is the same rule scenes follow, and for the same reason: recalling
 * something must never move a control you were not touching. A pin does not
 * change the sound at all, it only decides what is visible.
 *
 * Eight is a deliberate cap. It is what fits in two readable rows at stage
 * distance, and an unbounded list would quietly turn the Perform view back into
 * the rack it exists to avoid.
 */
class PinnedControls final
{
public:
    static constexpr int kMaxPins = 8;

    struct Pin
    {
        std::string effectId;
        std::string parameterId;

        bool isValid() const noexcept { return ! effectId.empty() && ! parameterId.empty(); }

        bool operator== (const Pin& other) const noexcept
        {
            return effectId == other.effectId && parameterId == other.parameterId;
        }
    };

    int getNumPins() const noexcept { return count; }
    const Pin& getPin (int index) const noexcept { return pins[(size_t) juce::jlimit (0, kMaxPins - 1, index)]; }

    bool isPinned (const std::string& effectId, const std::string& parameterId) const noexcept;

    /** Appends a pin. Fails when it is already pinned or the list is full. */
    bool add (const std::string& effectId, const std::string& parameterId);

    /** Removes a pin, closing the gap. Returns false when it was not pinned. */
    bool remove (const std::string& effectId, const std::string& parameterId);

    /** Adds when absent, removes when present. Returns the new pinned state. */
    bool toggle (const std::string& effectId, const std::string& parameterId);

    void clear() noexcept { count = 0; }

    /** [{ "effect": ..., "parameter": ... }, ...] */
    juce::var toVar() const;

    /**
     * Replaces the list from a stored array.
     *
     * Unknown or unregistered ids are dropped rather than kept: a preset written
     * when an effect existed, opened in a build where it does not, would
     * otherwise put a knob on the stage screen that cannot be drawn. Pass a null
     * registry to skip that check.
     */
    void fromVar (const juce::var& value, const milodikfx::api::ParameterRegistry* registry = nullptr);

private:
    std::array<Pin, kMaxPins> pins {};
    int count = 0;
};
} // namespace milodikfx::preset
