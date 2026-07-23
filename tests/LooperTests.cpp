#include <JuceHeader.h>

#include "dsp/LooperProcessor.h"

//==============================================================================
class LooperProcessorTests final : public juce::UnitTest
{
public:
    LooperProcessorTests() : juce::UnitTest ("LooperProcessor", "dsp") {}

    using Looper = milodikfx::dsp::LooperProcessor;
    using Action = Looper::Action;
    using State = Looper::State;

    static constexpr int kBlock = 256;

    /** Fills a block with a constant, runs it through, and returns the peak out. */
    static float runBlock (Looper& looper, float inputValue)
    {
        juce::AudioBuffer<float> buffer (2, kBlock);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kBlock; ++i)
                buffer.setSample (ch, i, inputValue);

        looper.processBlock (buffer);

        return buffer.getMagnitude (0, kBlock);
    }

    void runTest() override
    {
        beginTest ("Records a phrase, closes the loop, and plays it back");
        {
            Looper looper;
            looper.prepareToPlay (48000.0, kBlock, 2);

            expect (looper.getState() == State::empty);
            expect (! looper.hasLoop());

            // First press starts recording; the defining pass makes no playback.
            looper.requestAction (Action::record);
            runBlock (looper, 0.5f);
            expect (looper.getState() == State::recording);

            // Second press closes the loop; the same block already plays it back.
            looper.requestAction (Action::record);
            const auto played = runBlock (looper, 0.0f);
            expect (looper.getState() == State::playing);
            expect (looper.hasLoop());
            expectWithinAbsoluteError (played, 0.5f, 0.02f);
            expectWithinAbsoluteError (looper.getLoopSeconds(), (float) kBlock / 48000.0f, 0.001f);

            // It keeps playing on the next pass, too.
            const auto again = runBlock (looper, 0.0f);
            expectWithinAbsoluteError (again, 0.5f, 0.02f);
        }

        beginTest ("Overdub layers a second pass on top");
        {
            Looper looper;
            looper.prepareToPlay (48000.0, kBlock, 2);

            looper.requestAction (Action::record);
            runBlock (looper, 0.5f);
            looper.requestAction (Action::record); // close -> playing
            runBlock (looper, 0.0f);

            // Overdub 0.3 on top of the 0.5 loop.
            looper.requestAction (Action::record);
            runBlock (looper, 0.3f);
            expect (looper.getState() == State::overdubbing);

            // Back to plain playback; the loop is now 0.5 + 0.3.
            looper.requestAction (Action::record);
            const auto layered = runBlock (looper, 0.0f);
            expect (looper.getState() == State::playing);
            expectWithinAbsoluteError (layered, 0.8f, 0.03f);
        }

        beginTest ("Stop halts playback but keeps the loop");
        {
            Looper looper;
            looper.prepareToPlay (48000.0, kBlock, 2);

            looper.requestAction (Action::record);
            runBlock (looper, 0.5f);
            looper.requestAction (Action::record);
            runBlock (looper, 0.0f);

            looper.requestAction (Action::stop);
            const auto silent = runBlock (looper, 0.0f);
            expect (looper.getState() == State::stopped);
            expect (looper.hasLoop());
            expectWithinAbsoluteError (silent, 0.0f, 0.001f);

            // Play resumes it.
            looper.requestAction (Action::play);
            const auto resumed = runBlock (looper, 0.0f);
            expect (looper.getState() == State::playing);
            expectWithinAbsoluteError (resumed, 0.5f, 0.02f);
        }

        beginTest ("Clear empties the loop");
        {
            Looper looper;
            looper.prepareToPlay (48000.0, kBlock, 2);

            looper.requestAction (Action::record);
            runBlock (looper, 0.5f);
            looper.requestAction (Action::record);
            runBlock (looper, 0.0f);
            expect (looper.hasLoop());

            looper.requestAction (Action::clear);
            const auto afterClear = runBlock (looper, 0.0f);
            expect (looper.getState() == State::empty);
            expect (! looper.hasLoop());
            expectWithinAbsoluteError (afterClear, 0.0f, 0.001f);
        }

        beginTest ("Playback level scales the loop, live signal passes untouched");
        {
            Looper looper;
            looper.prepareToPlay (48000.0, kBlock, 2);

            looper.requestAction (Action::record);
            runBlock (looper, 0.5f);
            looper.requestAction (Action::record);
            runBlock (looper, 0.0f);

            looper.setLevelPercent (0.0f);
            const auto muted = runBlock (looper, 0.0f);
            expectWithinAbsoluteError (muted, 0.0f, 0.001f);

            // The live signal is never attenuated by the looper, only the loop.
            const auto live = runBlock (looper, 0.2f);
            expectWithinAbsoluteError (live, 0.2f, 0.001f);

            looper.setLevelPercent (100.0f);
            const auto full = runBlock (looper, 0.0f);
            expectWithinAbsoluteError (full, 0.5f, 0.02f);
        }

        beginTest ("Its output is clamped, since it runs after the limiter");
        {
            Looper looper;
            looper.prepareToPlay (48000.0, kBlock, 2);

            // Record a hot loop, then overdub hot again: the sum would exceed full
            // scale, and the looper must clamp rather than pass it on.
            looper.requestAction (Action::record);
            runBlock (looper, 0.9f);
            looper.requestAction (Action::record);
            runBlock (looper, 0.0f);
            looper.requestAction (Action::record); // overdub
            const auto over = runBlock (looper, 0.9f);
            expect (over <= 1.0f, "looper output left full scale: " + juce::String (over));
        }
    }
};

static LooperProcessorTests looperProcessorTests;
