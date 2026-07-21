#include "api/UndoHistory.h"

namespace milodikfx::api
{
namespace
{
/** Applies a snapshot held as JSON text. Returns false if it will not parse. */
bool applyText (const ParameterRegistry& registry, const juce::String& text)
{
    juce::var parsed;

    if (! juce::JSON::parse (text, parsed).wasOk() || ! parsed.isObject())
        return false;

    registry.applyState (parsed);
    return true;
}
} // namespace

UndoHistory::UndoHistory (const ParameterRegistry& registryToUse)
    : registry (registryToUse)
{
}

juce::String UndoHistory::toText (const juce::var& state)
{
    return juce::JSON::toString (state, true);
}

void UndoHistory::reset()
{
    const juce::ScopedLock scoped (lock);

    undoStack.clear();
    redoStack.clear();
    baseline = toText (registry.captureState());
}

bool UndoHistory::commitIfChanged()
{
    const auto current = toText (registry.captureState());

    const juce::ScopedLock scoped (lock);

    if (current == baseline)
        return false;

    undoStack.push_back (baseline);

    while ((int) undoStack.size() > kMaxDepth)
        undoStack.pop_front();

    // A new edit is a new branch; whatever was ahead is no longer reachable.
    redoStack.clear();

    baseline = current;

    return true;
}

bool UndoHistory::undo()
{
    juce::String target;

    {
        const juce::ScopedLock scoped (lock);

        if (undoStack.empty())
            return false;

        target = undoStack.back();
        undoStack.pop_back();
        redoStack.push_back (baseline);
    }

    if (! applyText (registry, target))
        return false;

    // Re-read rather than trusting the snapshot: a parameter can clamp what it
    // was given, and a baseline that disagreed with the chain would make the
    // next commit record a step nobody took.
    const juce::ScopedLock scoped (lock);
    baseline = toText (registry.captureState());

    return true;
}

bool UndoHistory::redo()
{
    juce::String target;

    {
        const juce::ScopedLock scoped (lock);

        if (redoStack.empty())
            return false;

        target = redoStack.back();
        redoStack.pop_back();
        undoStack.push_back (baseline);
    }

    if (! applyText (registry, target))
        return false;

    const juce::ScopedLock scoped (lock);
    baseline = toText (registry.captureState());

    return true;
}

bool UndoHistory::canUndo() const
{
    const juce::ScopedLock scoped (lock);
    return ! undoStack.empty();
}

bool UndoHistory::canRedo() const
{
    const juce::ScopedLock scoped (lock);
    return ! redoStack.empty();
}

int UndoHistory::getUndoDepth() const
{
    const juce::ScopedLock scoped (lock);
    return (int) undoStack.size();
}

int UndoHistory::getRedoDepth() const
{
    const juce::ScopedLock scoped (lock);
    return (int) redoStack.size();
}
} // namespace milodikfx::api
