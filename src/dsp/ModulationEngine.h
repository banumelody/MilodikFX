#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <string>

#include "api/ParameterRegistry.h"

namespace milodikfx::dsp
{
/**
 * Modifiers: a source (an LFO or an envelope follower) sweeps a parameter
 * between two values, per audio block, the way a Fractal FM9 modifier does.
 * A tremolo is the master level swept by an LFO; an auto-wah is the contour
 * frequency swept by the envelope of your picking.
 *
 * Realtime discipline, because this runs on the audio thread every block:
 *
 * - Fixed four slots, no allocation ever. Configuration comes in through atomics;
 *   the slot's target is an atomic pointer published with release/acquire so the
 *   audio thread never reads a half-written descriptor.
 * - It writes the swept value through the parameter's own `set` closure, which is
 *   a plain atomic store into the processor -- the same store a MIDI CC does, not
 *   the registry's setter with its change notification. Nothing is allocated and
 *   no lock is taken.
 * - The phase and envelope state belong to the audio thread alone. A reconfigure
 *   raises a `needsReset` flag the audio thread clears itself, so the control
 *   thread never writes audio-thread state.
 * - Switching a modifier off restores the parameter to the value it had before
 *   modulation began -- and the restore is done by the audio thread the first
 *   block after the slot goes inactive, so it cannot fight a write in flight.
 *
 * It does not change the user's stored value: while a parameter is modulated its
 * knob is inert in the UI (the modifier owns it), and the value returns when the
 * modifier is removed. A base-plus-offset model, where the knob still sets a
 * centre the modulation rides on, is a later refinement.
 */
class ModulationEngine final
{
public:
    static constexpr int kMaxModifiers = 4;

    enum class Source
    {
        lfoSine = 0,
        lfoTriangle = 1,
        lfoSquare = 2,
        envelope = 3
    };

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /**
     * Configures a slot (control thread). `target` must outlive the modifier and
     * be a numeric parameter with get/set. Returns false for a bad slot or target.
     * effectId/parameterId are kept only so the API can report the binding.
     */
    bool setModifier (int slot,
                      const milodikfx::api::ParameterDescriptor* target,
                      std::string effectId,
                      std::string parameterId,
                      Source source,
                      float low,
                      float high,
                      float rateHz) noexcept;

    /** Switches a slot off; the audio thread restores the parameter next block. */
    void clearModifier (int slot) noexcept;

    /** Read-only snapshot of a slot, for the API (control thread). */
    struct ModifierInfo
    {
        bool active = false;
        std::string effectId;
        std::string parameterId;
        int source = 0;
        float low = 0.0f;
        float high = 0.0f;
        float rateHz = 1.0f;
    };

    ModifierInfo getModifier (int slot) const;

    /**
     * If a parameter is currently modulated, writes its pre-modulation ("base")
     * value to `out` and returns true; otherwise returns false. Persistence uses
     * this so saving while a modifier is sweeping stores the value the parameter
     * will return to, not the swept sample it happened to be on. Control thread.
     */
    bool getBaseValue (const std::string& effectId, const std::string& parameterId, float& out) const;

    static bool isValidSlot (int slot) noexcept { return slot >= 0 && slot < kMaxModifiers; }

    /**
     * Audio thread: advance every active modifier by one block and write its
     * swept value. `inputLevel` is the block's input magnitude, for the envelope
     * follower. Allocation- and lock-free.
     */
    void process (float inputLevel, int numSamples) noexcept;

private:
    struct Slot
    {
        std::atomic<bool> active { false };
        std::atomic<bool> needsReset { false };
        std::atomic<const milodikfx::api::ParameterDescriptor*> target { nullptr };
        std::atomic<int> source { 0 };
        std::atomic<float> low { 0.0f };
        std::atomic<float> high { 0.0f };
        std::atomic<float> rateHz { 1.0f };
        std::atomic<float> restoreValue { 0.0f };

        // Audio-thread-owned state.
        double phase = 0.0;
        float env = 0.0f;
        bool wasActive = false;

        // Control-thread-only, for the API. Never touched by the audio thread.
        std::string effectId;
        std::string parameterId;
    };

    std::array<Slot, kMaxModifiers> slots;
    double sampleRate = 48000.0;
};
} // namespace milodikfx::dsp
