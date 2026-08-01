#include <JuceHeader.h>

#include "api/LevelsHandler.h"

namespace
{
/** Reads a number back out of the JSON the handler actually serves. */
float field (const LevelsHandler& handler, const char* name)
{
    const auto response = handler.handleGet ("/api/levels", {});
    juce::var parsed;

    if (! juce::JSON::parse (response.body, parsed).wasOk())
        return -999.0f;

    return (float) (double) parsed[juce::Identifier (name)];
}

/**
 * Per-channel metering.
 *
 * Until v0.28 two bars would have moved together forever -- everything was mono
 * and duplicated. Pan per path, the stereo cabinet mode and the L/R split
 * changed that, and the combined figure is the *louder* of the two, so the
 * quieter side is currently invisible rather than merely un-itemised.
 */
class MeterTests final : public juce::UnitTest
{
public:
    MeterTests() : juce::UnitTest ("Metering", "milodikfx") {}

    void runTest() override
    {
        beginTest ("Each channel is reported on its own");
        {
            LevelsHandler handler;
            handler.updateLevels (-6.0f, -20.0f, -8.0f, -22.0f, -3.0f, -15.0f);

            expectWithinAbsoluteError (field (handler, "inputLevelL"), -6.0f, 0.01f);
            expectWithinAbsoluteError (field (handler, "inputLevelR"), -20.0f, 0.01f);
            expectWithinAbsoluteError (field (handler, "chainInputLevelL"), -8.0f, 0.01f);
            expectWithinAbsoluteError (field (handler, "chainInputLevelR"), -22.0f, 0.01f);
            expectWithinAbsoluteError (field (handler, "outputLevelL"), -3.0f, 0.01f);
            expectWithinAbsoluteError (field (handler, "outputLevelR"), -15.0f, 0.01f);
        }

        beginTest ("The combined figure is the louder side, and it stays");
        {
            // A client that never asked for channels keeps working unchanged --
            // including tests/smoke.ps1, which asserts on these names.
            LevelsHandler handler;
            handler.updateLevels (-6.0f, -20.0f, -8.0f, -22.0f, -15.0f, -3.0f);

            expectWithinAbsoluteError (field (handler, "inputLevel"), -6.0f, 0.01f);
            expectWithinAbsoluteError (field (handler, "chainInputLevel"), -8.0f, 0.01f);

            // Deliberately louder on the right here: the maximum, not channel 0.
            expectWithinAbsoluteError (field (handler, "outputLevel"), -3.0f, 0.01f);
        }

        beginTest ("A mono signal reports the same on both sides");
        {
            LevelsHandler handler;
            handler.updateLevels (-12.0f, -12.0f, -12.0f, -12.0f, -9.0f, -9.0f);

            expectWithinAbsoluteError (field (handler, "outputLevelL"),
                                       field (handler, "outputLevelR"), 0.001f);
            expectWithinAbsoluteError (field (handler, "outputLevel"), -9.0f, 0.01f);
        }

        beginTest ("The old three-argument call still works");
        {
            // Kept so the plugin and any other caller can be moved over one at a
            // time rather than all at once.
            LevelsHandler handler;
            handler.updateLevels (-6.0f, -4.0f, -2.0f);

            expectWithinAbsoluteError (field (handler, "inputLevel"), -6.0f, 0.01f);
            expectWithinAbsoluteError (field (handler, "outputLevel"), -2.0f, 0.01f);

            // With no channel information, both sides carry the same figure --
            // which is exactly true of a mono rig and honest about the rest.
            expectWithinAbsoluteError (field (handler, "outputLevelL"), -2.0f, 0.01f);
            expectWithinAbsoluteError (field (handler, "outputLevelR"), -2.0f, 0.01f);
        }

        beginTest ("Silence reads as the floor on every field");
        {
            LevelsHandler handler;

            for (const auto* name : { "inputLevel", "outputLevel",
                                      "inputLevelL", "inputLevelR",
                                      "outputLevelL", "outputLevelR" })
                expect (field (handler, name) <= -99.0f,
                        juce::String (name) + " does not start at the floor");
        }
    }
};

static MeterTests meterTests;
} // namespace
