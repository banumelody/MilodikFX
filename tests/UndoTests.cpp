#include <JuceHeader.h>

#include "api/UndoHistory.h"
#include "dsp/ChainFactory.h"
#include "dsp/DSPChainManager.h"

class UndoHistoryTests final : public juce::UnitTest
{
public:
    UndoHistoryTests() : juce::UnitTest ("UndoHistory", "api") {}

    void runTest() override
    {
        milodikfx::dsp::DSPChainManager manager;
        const auto chain = milodikfx::dsp::buildGuitarChain (manager);

        milodikfx::api::ParameterRegistry registry;
        milodikfx::dsp::registerChainParameters (registry, chain, manager);

        const auto* drive = registry.findParameter ("overdrive", "drivePct");
        expect (drive != nullptr);

        if (drive == nullptr)
            return;

        auto setDrive = [&registry] (float value)
        {
            float applied = 0.0f;
            registry.setParameter ("overdrive", "drivePct", value, applied);
            return applied;
        };

        beginTest ("Nothing to undo to begin with");
        {
            milodikfx::api::UndoHistory history (registry);
            history.reset();

            expect (! history.canUndo());
            expect (! history.canRedo());
            expect (! history.undo());
            expect (! history.redo());
            expectEquals (history.getUndoDepth(), 0);
        }

        beginTest ("A commit with nothing changed records nothing");
        {
            // Otherwise a timer calling this every second would fill the stack
            // with identical entries and undo would appear to do nothing.
            milodikfx::api::UndoHistory history (registry);
            history.reset();

            expect (! history.commitIfChanged());
            expect (! history.commitIfChanged());
            expect (! history.canUndo());
        }

        beginTest ("Undo puts the value back");
        {
            milodikfx::api::UndoHistory history (registry);

            setDrive (10.0f);
            history.reset();

            setDrive (60.0f);
            expect (history.commitIfChanged());
            expect (history.canUndo());

            expect (history.undo());
            expectWithinAbsoluteError (drive->get(), 10.0f, 0.01f);
        }

        beginTest ("Redo puts it back again");
        {
            milodikfx::api::UndoHistory history (registry);

            setDrive (10.0f);
            history.reset();

            setDrive (60.0f);
            history.commitIfChanged();
            history.undo();

            expect (history.canRedo());
            expect (history.redo());
            expectWithinAbsoluteError (drive->get(), 60.0f, 0.01f);

            expect (! history.canRedo());
            expect (history.canUndo());
        }

        beginTest ("Several steps unwind in order");
        {
            milodikfx::api::UndoHistory history (registry);

            setDrive (10.0f);
            history.reset();

            for (const auto value : { 20.0f, 30.0f, 40.0f })
            {
                setDrive (value);
                history.commitIfChanged();
            }

            expectEquals (history.getUndoDepth(), 3);

            history.undo();
            expectWithinAbsoluteError (drive->get(), 30.0f, 0.01f);

            history.undo();
            expectWithinAbsoluteError (drive->get(), 20.0f, 0.01f);

            history.undo();
            expectWithinAbsoluteError (drive->get(), 10.0f, 0.01f);

            expect (! history.canUndo());
            expectEquals (history.getRedoDepth(), 3);
        }

        beginTest ("A new edit after undoing discards what was ahead");
        {
            // The branch you undid away from is no longer reachable, and
            // offering to redo into it would apply something unrelated.
            milodikfx::api::UndoHistory history (registry);

            setDrive (10.0f);
            history.reset();

            setDrive (50.0f);
            history.commitIfChanged();
            history.undo();

            expect (history.canRedo());

            setDrive (75.0f);
            history.commitIfChanged();

            expect (! history.canRedo());
            expect (history.canUndo());

            history.undo();
            expectWithinAbsoluteError (drive->get(), 10.0f, 0.01f);
        }

        beginTest ("An effect switch is part of a step too");
        {
            milodikfx::api::UndoHistory history (registry);

            registry.setEffectEnabled ("delay", false);
            history.reset();

            registry.setEffectEnabled ("delay", true);
            expect (history.commitIfChanged());

            expect (history.undo());
            expect (! registry.findEffect ("delay")->isEnabled());
        }

        beginTest ("A burst of changes collapses into one step");
        {
            // What the settle delay buys: dragging a knob writes a value per
            // frame, and one undo step each would mean fifty presses to get
            // back where you started.
            milodikfx::api::UndoHistory history (registry);

            setDrive (0.0f);
            history.reset();

            for (int i = 1; i <= 40; ++i)
                setDrive ((float) i);

            expect (history.commitIfChanged());
            expectEquals (history.getUndoDepth(), 1);

            history.undo();
            expectWithinAbsoluteError (drive->get(), 0.0f, 0.01f);
        }

        beginTest ("The stack stays bounded");
        {
            milodikfx::api::UndoHistory history (registry);

            setDrive (0.0f);
            history.reset();

            for (int i = 1; i <= milodikfx::api::UndoHistory::kMaxDepth + 20; ++i)
            {
                setDrive ((float) (i % 100));
                history.commitIfChanged();
            }

            expectEquals (history.getUndoDepth(), milodikfx::api::UndoHistory::kMaxDepth);
        }

        beginTest ("Undoing past a value the parameter clamped still settles");
        {
            // The baseline is re-read from the chain after applying, so a
            // parameter that refused what it was given cannot leave the history
            // convinced the chain is somewhere it is not.
            milodikfx::api::UndoHistory history (registry);

            const auto clamped = setDrive (99999.0f);
            history.reset();

            setDrive (25.0f);
            history.commitIfChanged();

            expect (history.undo());
            expectWithinAbsoluteError (drive->get(), clamped, 0.01f);

            // And no phantom step appears afterwards.
            expect (! history.commitIfChanged());
        }

        beginTest ("reset forgets everything and takes the chain as it stands");
        {
            milodikfx::api::UndoHistory history (registry);

            setDrive (10.0f);
            history.reset();

            setDrive (70.0f);
            history.commitIfChanged();
            expect (history.canUndo());

            history.reset();

            expect (! history.canUndo());
            expect (! history.canRedo());
            expect (! history.commitIfChanged());
            expectWithinAbsoluteError (drive->get(), 70.0f, 0.01f);
        }
    }
};

static UndoHistoryTests undoHistoryTests;
