#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include "dsp/AudioProcessorBase.h"

namespace milodikfx::dsp
{
class SplitProcessor;
class MixerProcessor;

class DSPChainManager final
{
public:
    DSPChainManager();

    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels);
    void processBlock (juce::AudioBuffer<float>& buffer);
    void reset();

    AudioProcessorBase* addProcessor (std::unique_ptr<AudioProcessorBase> processor);

    /**
     * Adds a processor that runs after the chain and after the bypass crossfade.
     *
     * For things that are mixed into the output rather than applied to the
     * guitar: the metronome belongs here because global bypass must not silence
     * the click, and because a click routed through the amp and cabinet would
     * not sound like a click.
     */
    AudioProcessorBase* addPostProcessor (std::unique_ptr<AudioProcessorBase> processor);

    void clear();

    void setBypassed (bool shouldBypass) noexcept;
    bool isBypassed() const noexcept;

    int getNumProcessors() const noexcept;

    //==============================================================================
    /** @name Processing order
     *
     * The order the chain runs in is a *value*, not a rearrangement of the
     * processors themselves: the objects are built once and never move. Order,
     * bus assignment and board placement live together in one `Snapshot`, which
     * the control thread writes under a spin lock and the audio thread copies
     * once, when it changes, under a **try**-lock.
     *
     * That is what makes reordering safe without ever blocking the audio
     * thread: it either takes the new snapshot whole or keeps the one it has,
     * never a mix of the two, and nothing is allocated or freed on either side.
     * Until v0.31 this was four bits per stage in a single `uint64`, which was
     * cheaper still but capped the chain at sixteen stages.
     *
     * A reorder is audible as a small discontinuity, and that is accepted rather
     * than hidden. The processors are stateful -- delay lines, filter memory --
     * so running the old and new orders together to crossfade between them would
     * advance that state twice. This is an editing gesture, not a performance
     * one; hardware units click here too.
     */
    /** @{ */

    /**
     * How many stages a chain may contain.
     *
     * Was sixteen while the order was packed four bits at a time into a single
     * atomic. v0.31's inventory builds twenty-four stages, so the packing is
     * gone; this is now just the size of the fixed arrays a snapshot holds.
     */
    static constexpr int kMaxOrderedProcessors = 32;

    /**
     * Marks stages at the head and tail of the chain as immovable.
     *
     * Two of them are, and neither is a matter of taste. The input trim has to
     * stay first because the input meter reports what the chain receives as
     * `inputDb + trimDb` rather than measuring twice -- move the trim and that
     * figure quietly becomes a lie. The master stage has to stay last because it
     * carries the safety limiter and the final clamp, and anything after it
     * would be unprotected.
     */
    void setFixedStages (int leadingFixed, int trailingFixed) noexcept;

    /**
     * Applies a processing order, given as processor indices in the order they
     * should run.
     *
     * Refuses anything that is not a complete permutation, and refuses to move a
     * fixed stage. Safe from any thread.
     */
    bool setOrder (const std::vector<int>& order) noexcept;

    /** The order currently in force, as processor indices. */
    std::vector<int> getOrder() const;

    /** @} */

    //==============================================================================
    /** @name Parallel paths
     *
     * Between a SplitProcessor and a MixerProcessor the chain runs two buffers,
     * and each stage in between is assigned to one of them. Modelled on Logic's
     * Pedalboard: a stage is *assigned* to a bus, never duplicated onto one, so
     * there is still exactly one overdrive and the registry's flat id set holds.
     *
     * The assignment travels inside the same snapshot as the order, so a block
     * can never see a new order with a stale bus assignment.
     */
    /** @{ */

    /** Tells the manager which stages orchestrate the split. Build time only. */
    void setParallelStages (SplitProcessor* split, MixerProcessor* mixer) noexcept;

    /** false = path A, true = path B. Ignored for stages outside the split. */
    void setStageOnBusB (int processorIndex, bool onBusB) noexcept;
    bool isStageOnBusB (int processorIndex) const noexcept;

    /** @} */

    //==============================================================================
    /** @name Board placement
     *
     * A stage that is not on the board is not in the chain at all: the run loop
     * skips it, so the result is bit-identical to a chain built without it.
     *
     * That is deliberately **not** the same as bypassing it. A bypassed delay
     * still runs, which is what keeps its spillover tail decaying after the
     * footswitch -- the whole point of bypass. A delay that is off the board has
     * no tail to keep, because it is not there.
     *
     * Carried in the same snapshot as the order and the bus assignment.
     * Everything starts placed, so a build that never calls this behaves as it
     * always did.
     */
    /** @{ */

    /** Refuses to remove a fixed stage; the limiter cannot be taken off. */
    void setStagePlaced (int processorIndex, bool placed) noexcept;
    bool isStagePlaced (int processorIndex) const noexcept;

    /** @} */

    // Find the first processor of type T in the chain (uses RTTI/dynamic_cast)
    template<typename T>
    T* findProcessor() noexcept
    {
        for (auto& p : processors)
        {
            if (auto* t = dynamic_cast<T*>(p.get()))
                return t;
        }

        for (auto& p : postProcessors)
        {
            if (auto* t = dynamic_cast<T*>(p.get()))
                return t;
        }

        return nullptr;
    }

private:
    void processChain (juce::AudioBuffer<float>& buffer);

    // The dry copy used to crossfade in and out of bypass. Allocated once, at a
    // size no realistic device block will exceed, so toggling bypass never
    // allocates and prepareToPlay never moves memory the audio thread is reading.
    static constexpr int kMaxChannels = 2;
    static constexpr int kMaxBlockSize = 8192;

    /** Crossfade length, in seconds, when entering or leaving bypass. */
    static constexpr double kBypassFadeSeconds = 0.01;

    /**
     * Everything the run loop needs for one block, in one object.
     *
     * Order, bus assignment and board placement used to be three separate
     * atomics, which left a window where a block could see a new order with an
     * old placement. Keeping them together closes it, and is what allows more
     * than sixteen stages -- a permutation of twenty-four no longer fits in a
     * uint64 however it is packed.
     */
    struct Snapshot
    {
        int count = 0;
        std::array<juce::int8, kMaxOrderedProcessors> order {};
        std::array<bool, kMaxOrderedProcessors> onBusB {};
        std::array<bool, kMaxOrderedProcessors> placed {};
    };

    /** Rebuilds `pending` from the current stage list and bumps the version. */
    void publishIdentity();

    /**
     * Publishes whatever the control thread has just written into `pending`.
     *
     * Callers must already hold `snapshotLock`.
     */
    void publishLocked() noexcept;

    std::vector<std::unique_ptr<AudioProcessorBase>> processors;
    std::vector<std::unique_ptr<AudioProcessorBase>> postProcessors;

    int leadingFixedStages = 0;
    int trailingFixedStages = 0;

    /**
     * How the snapshot reaches the audio thread.
     *
     * The control thread mutates `pending` under `snapshotLock` and bumps
     * `snapshotVersion`. The audio thread notices the version moved and copies
     * `pending` into its own `live` copy under a **try**-lock: it never blocks,
     * and if the lock happens to be held it simply keeps using the copy it
     * already has and picks the change up on the next block.
     *
     * That is what makes this realtime-safe without a garbage-collected pointer
     * handoff: nothing is allocated, nothing is freed, and the audio thread
     * always has a complete and self-consistent snapshot to run from. The copy
     * itself only happens when something actually changed -- an editing
     * gesture, not a per-block cost.
     */
    mutable juce::SpinLock snapshotLock;
    Snapshot pending;                              // guarded by snapshotLock
    std::atomic<juce::uint32> snapshotVersion { 0 };

    Snapshot live;                                 // audio thread only
    juce::uint32 liveVersion = 0;

    /** True when the index is inside the fixed head or tail of the chain. */
    bool isFixedStage (int processorIndex) const noexcept;

    // Recognised by pointer so a reorder cannot leave a stale position behind.
    SplitProcessor* splitStage = nullptr;
    MixerProcessor* mixerStage = nullptr;

    /** True when a split stage exists and is switched on. */
    bool splitIsActive() const noexcept;
    void divideAtSplit (juce::AudioBuffer<float>& pathA, juce::AudioBuffer<float>& b) noexcept;
    void combineAtMixer (juce::AudioBuffer<float>& pathA, const juce::AudioBuffer<float>& b) noexcept;

    /** Path B's buffer. Allocated once, never resized while audio runs. */
    juce::AudioBuffer<float> pathB;

    std::atomic<bool> bypassed { false };
    double currentSampleRate = 0.0;
    int currentSamplesPerBlock = 0;
    int currentNumChannels = 0;
    bool prepared = false;

    juce::AudioBuffer<float> dryCopy;

    // 1 = fully processed, 0 = fully bypassed. Audio-thread owned.
    float wetGain = 1.0f;
    float fadeStep = 1.0f;
};
} // namespace milodikfx::dsp
