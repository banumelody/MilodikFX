#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>

#include "dsp/AudioProcessorBase.h"
#include "dsp/Biquad.h"

namespace milodikfx::dsp
{
/**
 * Where the signal becomes two paths.
 *
 * Modelled on Logic Pro's Pedalboard rather than on Fractal's grid, and the
 * difference is the whole reason this was affordable: **a stage is assigned to a
 * bus, never duplicated onto one.** There is still exactly one overdrive, one
 * EQ, one delay -- each simply lives on path A or path B. So the registry keeps
 * its flat, stable id set (`overdrive.drivePct` means one thing), presets do not
 * break, and host automation is untouched. Duplicating blocks would have meant
 * per-instance ids everywhere, which is a different project.
 *
 * This is a stage in the chain rather than a property of it, so it is dragged
 * around like anything else -- moving it is how you decide where the parallel
 * section begins, exactly as Pedalboard's Router works.
 *
 * It does not process the signal itself. `DSPChainManager` recognises it and
 * does the buffer work; this holds the settings and the crossover filters.
 *
 * Disabled by default, and when disabled the chain is serial and bit-identical
 * to a build without it.
 */
class SplitProcessor final : public AudioProcessorBase
{
public:
    enum class Mode
    {
        /** The same signal down both paths. */
        even = 0,

        /**
         * Low frequencies to A, high to B.
         *
         * The classic bass move -- keep the low end clean and drive only the
         * top -- and the equivalent of Pedalboard's Freq mode and Fractal's
         * Crossover block. Linkwitz-Riley (two cascaded Butterworth sections),
         * so the two halves sum back to flat when nothing else is in the way.
         */
        crossover = 1
    };

    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels) override;
    void processBlock (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setEnabled (bool shouldEnable) noexcept { enabled.store (shouldEnable, std::memory_order_relaxed); }
    bool isEnabled() const noexcept { return enabled.load (std::memory_order_relaxed); }

    void setMode (Mode mode) noexcept { splitMode.store ((int) mode, std::memory_order_relaxed); }
    Mode getMode() const noexcept { return (Mode) splitMode.load (std::memory_order_relaxed); }

    void setFrequencyHz (float hz) noexcept;
    float getFrequencyHz() const noexcept { return frequencyHz.load (std::memory_order_relaxed); }

    static constexpr float kMinFrequencyHz = 60.0f;
    static constexpr float kMaxFrequencyHz = 2000.0f;

    /**
     * Fills `pathB` from `pathA`, and leaves `pathA` holding what path A should
     * carry. Called by DSPChainManager, on the audio thread, at the split point.
     */
    void divide (juce::AudioBuffer<float>& pathA, juce::AudioBuffer<float>& pathB) noexcept;

private:
    static constexpr int kMaxChannels = 2;

    /** Two cascaded second-order sections per path: Linkwitz-Riley 4th order. */
    static constexpr int kSections = 2;

    void rebuildIfNeeded() noexcept;

    double sampleRate = 48000.0;

    std::atomic<bool> enabled { false };
    std::atomic<int> splitMode { (int) Mode::even };
    std::atomic<float> frequencyHz { 250.0f };

    // Audio-thread owned.
    BiquadCoeffs lowCoeffs {};
    BiquadCoeffs highCoeffs {};
    float builtForFrequency = -1.0f;
    double builtForRate = 0.0;

    std::array<std::array<BiquadState, kSections>, kMaxChannels> lowStates {};
    std::array<std::array<BiquadState, kSections>, kMaxChannels> highStates {};
};
} // namespace milodikfx::dsp
