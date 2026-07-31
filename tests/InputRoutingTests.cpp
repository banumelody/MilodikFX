#include <JuceHeader.h>

#include "audio/AudioDeviceController.h"

namespace
{
using milodikfx::audio::AudioDeviceController;

/** Builds the active-channel mask the way a device reports it. */
juce::BigInteger activeMask (std::initializer_list<int> channels)
{
    juce::BigInteger mask;

    for (const auto channel : channels)
        mask.setBit (channel);

    return mask;
}

/**
 * The port mapping, tested without hardware.
 *
 * This is the one piece of v0.29 whose failure mode is silence rather than a
 * crash: pick the wrong position and the engine reads a jack nobody plugged
 * into, with no error anywhere. Every case below is one a Scarlett 4i4 can
 * actually produce.
 */
class InputRoutingTests final : public juce::UnitTest
{
public:
    InputRoutingTests() : juce::UnitTest ("Input routing", "milodikfx") {}

    void runTest() override
    {
        const juce::StringArray fourIn { "Input 1", "Input 2", "Input 3", "Input 4" };

        beginTest ("With every channel active, position is just the index");
        {
            const auto all = activeMask ({ 0, 1, 2, 3 });

            expectEquals (AudioDeviceController::inputPositionFor (fourIn, all, "Input 1", -1), 0);
            expectEquals (AudioDeviceController::inputPositionFor (fourIn, all, "Input 2", -1), 1);
            expectEquals (AudioDeviceController::inputPositionFor (fourIn, all, "Input 3", -1), 2);
            expectEquals (AudioDeviceController::inputPositionFor (fourIn, all, "Input 4", -1), 3);
        }

        beginTest ("Inactive channels do not advance the position");
        {
            // The trap the whole design exists to avoid. With only ports 2 and 4
            // streaming, the callback's array is two entries long: port 2 is at
            // 0 and port 4 at 1. Reading them at their *indices* -- 1 and 3 --
            // means one wrong jack and one read past the end.
            const auto sparse = activeMask ({ 1, 3 });

            expectEquals (AudioDeviceController::inputPositionFor (fourIn, sparse, "Input 2", -1), 0);
            expectEquals (AudioDeviceController::inputPositionFor (fourIn, sparse, "Input 4", -1), 1);
        }

        beginTest ("A port that is not streaming falls back rather than going silent");
        {
            const auto sparse = activeMask ({ 1, 3 });

            // Port 1 exists on the device but is not active, so it has no
            // position at all -- the caller's fallback is what should come back.
            expectEquals (AudioDeviceController::inputPositionFor (fourIn, sparse, "Input 1", 0), 0);
            expectEquals (AudioDeviceController::inputPositionFor (fourIn, sparse, "Input 3", 7), 7);
        }

        beginTest ("A name from another interface falls back");
        {
            // Exactly what happens when the settings file was written on the
            // 4i4 and the app is opened on a laptop's built-in input.
            const juce::StringArray builtIn { "Microphone" };
            const auto one = activeMask ({ 0 });

            expectEquals (AudioDeviceController::inputPositionFor (builtIn, one, "Input 2", 1), 1);
            expectEquals (AudioDeviceController::inputPositionFor (builtIn, one, "Microphone", -1), 0);
        }

        beginTest ("No preference means the fallback, not the first match");
        {
            const auto all = activeMask ({ 0, 1, 2, 3 });

            // A fresh install has no stored names, and both channels must still
            // land somewhere sensible: L on 0 and R on 1.
            expectEquals (AudioDeviceController::inputPositionFor (fourIn, all, {}, 0), 0);
            expectEquals (AudioDeviceController::inputPositionFor (fourIn, all, {}, 1), 1);
        }

        beginTest ("Both channels may name the same port");
        {
            // A mono source that should hit both sides of the chain: legal, and
            // it must not be quietly rewritten into two different ports.
            const auto all = activeMask ({ 0, 1, 2, 3 });

            const auto left = AudioDeviceController::inputPositionFor (fourIn, all, "Input 3", 0);
            const auto right = AudioDeviceController::inputPositionFor (fourIn, all, "Input 3", 1);

            expectEquals (left, 2);
            expectEquals (right, 2);
        }

        beginTest ("A device with no inputs yields the fallback for everything");
        {
            const juce::StringArray none;
            const juce::BigInteger empty;

            expectEquals (AudioDeviceController::inputPositionFor (none, empty, "Input 1", -1), -1);
            expectEquals (AudioDeviceController::inputPositionFor (none, empty, {}, -1), -1);
        }

        beginTest ("Duplicate channel names resolve to the first one");
        {
            // Some drivers report the same label twice. Any answer is arguably
            // defensible; what matters is that it is deterministic.
            const juce::StringArray duplicated { "In", "In", "In" };
            const auto all = activeMask ({ 0, 1, 2 });

            expectEquals (AudioDeviceController::inputPositionFor (duplicated, all, "In", -1), 0);
        }
    }
};

static InputRoutingTests inputRoutingTests;
} // namespace
