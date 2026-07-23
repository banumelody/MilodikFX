#include "dsp/LooperProcessor.h"

#include <cmath>

namespace milodikfx::dsp
{
void LooperProcessor::prepareToPlay (double sampleRateIn, int, int)
{
    sampleRate = sampleRateIn > 0.0 ? sampleRateIn : 48000.0;

    // Allocated once, here, never in the callback. Two channels is the engine's
    // width; a mono rig still writes both from the same signal upstream.
    maxSamples = (int) std::ceil (kMaxSeconds * sampleRate);
    loop.setSize (2, maxSamples, false, true, false);
    loop.clear();

    reset();

    prepared = true;
}

void LooperProcessor::reset()
{
    // Not the buffer contents: clearing 60 s of stereo audio is far too much for
    // the audio thread, and length 0 means nothing is read anyway.
    state = State::empty;
    loopLength = 0;
    recordPos = 0;
    playPos = 0;

    pendingAction.store ((int) Action::none, std::memory_order_relaxed);
    publishedState.store ((int) State::empty, std::memory_order_relaxed);
    publishedLoopSamples.store (0, std::memory_order_relaxed);
    publishedPosition.store (0, std::memory_order_relaxed);
}

void LooperProcessor::applyAction (Action action) noexcept
{
    switch (action)
    {
        case Action::record:
            switch (state)
            {
                case State::empty:
                case State::stopped:
                    if (state == State::stopped && loopLength > 0)
                    {
                        // Resume playback; a second press then drops into overdub.
                        state = State::playing;
                    }
                    else
                    {
                        state = State::recording;
                        recordPos = 0;
                        loopLength = 0;
                    }
                    break;

                case State::recording:
                    // Close the loop on the second press.
                    loopLength = recordPos;
                    if (loopLength > 0)
                    {
                        state = State::playing;
                        playPos = 0;
                    }
                    else
                    {
                        state = State::empty;
                    }
                    break;

                case State::playing:
                    state = State::overdubbing;
                    break;

                case State::overdubbing:
                    state = State::playing;
                    break;
            }
            break;

        case Action::stop:
            if (state == State::recording)
            {
                loopLength = recordPos; // keep what was captured
                state = loopLength > 0 ? State::stopped : State::empty;
            }
            else if (state == State::playing || state == State::overdubbing)
            {
                state = State::stopped;
            }
            break;

        case Action::play:
            if (loopLength > 0)
                state = State::playing;
            break;

        case Action::clear:
            state = State::empty;
            loopLength = 0;
            recordPos = 0;
            playPos = 0;
            break;

        case Action::none:
        default:
            break;
    }
}

void LooperProcessor::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (! prepared)
        return;

    applyAction ((Action) pendingAction.exchange ((int) Action::none, std::memory_order_relaxed));

    const auto numSamples = buffer.getNumSamples();
    const auto numCh = juce::jmin (buffer.getNumChannels(), loop.getNumChannels());

    if (numSamples > 0 && numCh > 0)
    {
        const auto level = juce::jlimit (0.0f, 1.0f, levelPercent.load (std::memory_order_relaxed) / 100.0f);

        auto* const* out = buffer.getArrayOfWritePointers();
        auto* const* loopData = loop.getArrayOfWritePointers();

        for (int i = 0; i < numSamples; ++i)
        {
            if (state == State::recording)
            {
                if (recordPos < maxSamples)
                {
                    for (int ch = 0; ch < numCh; ++ch)
                        loopData[ch][recordPos] = clampSample (out[ch][i]);

                    ++recordPos;
                    continue; // no playback on the defining pass
                }

                // The buffer is full: close the loop and fall through to playing.
                loopLength = recordPos;
                state = loopLength > 0 ? State::playing : State::empty;
                playPos = 0;
            }

            if ((state == State::playing || state == State::overdubbing) && loopLength > 0)
            {
                for (int ch = 0; ch < numCh; ++ch)
                {
                    const auto existing = loopData[ch][playPos];

                    if (state == State::overdubbing)
                        loopData[ch][playPos] = clampSample (existing + out[ch][i]);

                    // Play the loop (pre-overdub) under the live signal already in
                    // the buffer. Its own clamp, since this runs after the limiter.
                    out[ch][i] = clampSample (out[ch][i] + existing * level);
                }

                if (++playPos >= loopLength)
                    playPos = 0;
            }
        }
    }

    publishedState.store ((int) state, std::memory_order_relaxed);
    publishedLoopSamples.store (loopLength, std::memory_order_relaxed);
    publishedPosition.store (state == State::recording ? recordPos : playPos, std::memory_order_relaxed);
}
} // namespace milodikfx::dsp
