#pragma once

#include <JuceHeader.h>

#include <deque>

#include "api/ParameterRegistry.h"

namespace milodikfx::api
{
/**
 * Undo and redo over whole-chain snapshots.
 *
 * A snapshot is exactly what a preset stores, so nothing here needs to know
 * what the parameters are. Entries are committed by the caller once the chain
 * has been still for a moment rather than on every write: dragging a knob
 * produces a write per frame, and each of those becoming its own undo step
 * would mean pressing Ctrl+Z fifty times to get back where you started.
 *
 * Both stacks are guarded, because undo arrives on a connection thread while
 * the commit runs on the message thread.
 */
class UndoHistory final
{
public:
    /** Deep enough to cover a session's worth of tweaking, bounded so a long
        session cannot grow it without limit. */
    static constexpr int kMaxDepth = 50;

    explicit UndoHistory (const ParameterRegistry& registryToUse);

    /** Forgets everything and takes the current chain as the starting point. */
    void reset();

    /**
     * Records the chain as an undo step if it differs from the last one.
     *
     * Call when the chain has settled. A no-op when nothing moved, so calling
     * it on a timer costs a comparison and nothing else.
     */
    bool commitIfChanged();

    bool undo();
    bool redo();

    bool canUndo() const;
    bool canRedo() const;

    int getUndoDepth() const;
    int getRedoDepth() const;

private:
    /** JSON text, because that is what makes two snapshots comparable. */
    static juce::String toText (const juce::var& state);

    const ParameterRegistry& registry;

    mutable juce::CriticalSection lock;

    std::deque<juce::String> undoStack;
    std::deque<juce::String> redoStack;

    /** The chain as of the last committed step. */
    juce::String baseline;
};
} // namespace milodikfx::api
