#pragma once

#include <JuceHeader.h>

namespace milodikfx::preset
{
/**
 * Where the user's own files live.
 *
 * The app and the plugin have to agree on these to the character: a preset
 * saved in the standalone rig must be the same file the plugin lists, and an
 * impulse response dropped in once must be visible to both. They were spelled
 * out separately in each host until the plugin grew a library of its own, which
 * is exactly the kind of duplication that drifts silently -- one of them gets a
 * new subfolder and nobody notices until a preset is missing.
 */
struct UserPaths
{
    static juce::File root()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("MilodikFX");
    }

    static juce::File presets()           { return root().getChildFile ("Presets"); }
    static juce::File impulseResponses()  { return root().getChildFile ("ImpulseResponses"); }
    static juce::File namModels()         { return root().getChildFile ("NamModels"); }
};
} // namespace milodikfx::preset
