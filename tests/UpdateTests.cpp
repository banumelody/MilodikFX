#include <JuceHeader.h>

#include "api/UpdateHandler.h"

using milodikfx::api::isNewerVersion;

// The update check's own network call needs GitHub and is exercised by hand;
// the decision it drives -- is the tag on GitHub newer than this build -- is a
// pure function, and this is where every way it can be fooled gets pinned down.
class UpdateVersionTests final : public juce::UnitTest
{
public:
    UpdateVersionTests() : juce::UnitTest ("UpdateVersion") {}

    void runTest() override
    {
        beginTest ("A higher release is newer, a lower one is not");
        expect (isNewerVersion ("0.14.0", "0.15.0"));
        expect (! isNewerVersion ("0.15.0", "0.14.0"));

        beginTest ("The same version is not an update");
        expect (! isNewerVersion ("0.15.0", "0.15.0"));

        beginTest ("The leading v on a git tag is ignored");
        expect (isNewerVersion ("0.14.0", "v0.15.0"));
        expect (! isNewerVersion ("v0.15.0", "v0.15.0"));
        expect (isNewerVersion ("v0.14.0", "0.15.0"));

        beginTest ("Each component is compared in order, not lexically");
        // "0.9.0" vs "0.10.0" is the classic string-compare trap: '1' < '9'.
        expect (isNewerVersion ("0.9.0", "0.10.0"));
        expect (! isNewerVersion ("0.10.0", "0.9.0"));
        // A major bump beats a large minor.
        expect (isNewerVersion ("0.99.0", "1.0.0"));
        // A patch bump alone still counts.
        expect (isNewerVersion ("1.2.3", "1.2.4"));
        expect (! isNewerVersion ("1.2.4", "1.2.3"));

        beginTest ("A missing component reads as zero");
        expect (isNewerVersion ("1.2", "1.2.1"));
        expect (! isNewerVersion ("1.2.0", "1.2"));
        expect (! isNewerVersion ("1.2", "1.2.0"));

        beginTest ("A pre-release suffix on a component is tolerated");
        // getIntValue stops at the first non-digit, so "0-rc1" reads as 0.
        expect (! isNewerVersion ("0.15.0", "0.15.0-rc1"));
        expect (isNewerVersion ("0.15.0-rc1", "0.16.0"));

        beginTest ("Junk never claims to be an update over a real version");
        expect (! isNewerVersion ("0.15.0", ""));
        expect (! isNewerVersion ("0.15.0", "not-a-version"));
    }
};

static UpdateVersionTests updateVersionTests;
