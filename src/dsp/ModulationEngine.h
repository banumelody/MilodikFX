#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <functional>
#include <string>

#include "api/ParameterRegistry.h"

namespace milodikfx::dsp
{
/**
 * Modifiers: a source (an LFO, the envelope of your picking, or an expression
 * pedal) sweeps a parameter, per audio block, the way a Fractal FM9 modifier
 * does. A tremolo is the master level swept by an LFO; an auto-wah is the
 * contour frequency swept by the envelope; a wah is the same swept by a pedal.
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
 *
 * Base + offset. The knob is not dead while a parameter is modulated: it sets a
 * *centre* the modulation rides on. `effective = clamp(low + source*(high-low) +
 * baseOffset)`, where `baseOffset` shifts the whole sweep window and defaults to
 * zero -- so a modifier added with the panel's low/high sweeps exactly that range
 * until the knob is turned. Turning the knob writes a new centre (see setBase);
 * removing the modifier returns the parameter to that centre.
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
        envelope = 3,
        expression = 4
    };

    /** LFO rate locked to the tempo. 0 = free (use rateHz); otherwise a note value. */
    enum class SyncDivision
    {
        off = 0,
        whole = 1,        // 1/1
        half = 2,         // 1/2
        quarter = 3,      // 1/4
        eighth = 4,       // 1/8
        dottedEighth = 5, // 1/8.
        eighthTriplet = 6,// 1/8T
        sixteenth = 7     // 1/16
    };

    static constexpr int kNumSyncDivisions = 8;

    /** How a slot is configured, so the setter does not grow a dozen arguments. */
    struct Config
    {
        Source source = Source::lfoSine;
        float low = 0.0f;
        float high = 0.0f;
        float rateHz = 1.0f;
        /** Which CC an expression source follows. -1 leaves it neutral. */
        int expressionCc = -1;
        /** Locks an LFO to the tempo; ignored by the envelope and expression sources. */
        int syncDivision = 0;
        /** Shifts the sweep centre. 0 = sweep exactly low..high. */
        float baseOffset = 0.0f;
    };

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** The project tempo, for LFOs locked to a note division. Any thread. */
    void setBpm (float beatsPerMinute) noexcept { bpm.store (beatsPerMinute, std::memory_order_relaxed); }

    /**
     * Supplies live expression-pedal values (a MIDI CC as 0..1) to the audio
     * thread. Set once at wiring time and never reassigned, so the audio thread
     * only ever *calls* it -- the same discipline as the parameter set closures.
     */
    std::function<float (int cc)> expressionProvider;

    /**
     * Configures a slot (control thread). `target` must outlive the modifier and
     * be a numeric parameter with get/set. Returns false for a bad slot or target.
     * effectId/parameterId are kept only so the API can report the binding.
     */
    bool setModifier (int slot,
                      const milodikfx::api::ParameterDescriptor* target,
                      std::string effectId,
                      std::string parameterId,
                      const Config& config) noexcept;

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
        int expressionCc = -1;
        int syncDivision = 0;
        float baseOffset = 0.0f;
        /** The centre the knob shows, midpoint(low,high) + baseOffset. */
        float base = 0.0f;
    };

    ModifierInfo getModifier (int slot) const;

    /**
     * If a parameter is currently modulated, writes its centre ("base") value to
     * `out` and returns true; otherwise returns false. Persistence uses this so
     * saving while a modifier is sweeping stores the value the parameter rests at,
     * not the swept sample it happened to be on. The effects listing uses it so
     * the knob shows the centre. Control thread.
     */
    bool getBaseValue (const std::string& effectId, const std::string& parameterId, float& out) const;

    /**
     * If a parameter is modulated, recentres its sweep on `centre` (in parameter
     * units) and returns true; the knob writes through here instead of the
     * atomic, which the audio thread owns. Returns false when nothing owns it.
     */
    bool setBase (const std::string& effectId, const std::string& parameterId, float centre) noexcept;

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
        std::atomic<int> expressionCc { -1 };
        std::atomic<int> syncDivision { 0 };
        std::atomic<float> baseOffset { 0.0f };

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
    std::atomic<float> bpm { 120.0f };
};
} // namespace milodikfx::dsp
