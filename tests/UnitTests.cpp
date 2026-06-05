#include <JuceHeader.h>

class SettingsPersistenceTests final : public juce::UnitTest
{
public:
    SettingsPersistenceTests()
        : juce::UnitTest ("SettingsPersistence")
    {
    }

    void runTest() override
    {
        beginTest ("PropertiesFile saves and reloads values");

        auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory);
        auto file = tempDir.getNonexistentChildFile ("MilodikFXTest", ".settings");

        juce::PropertiesFile::Options options;
        options.applicationName = "MilodikFXTests";
        options.filenameSuffix = "settings";
        options.millisecondsBeforeSaving = -1;
        options.storageFormat = juce::PropertiesFile::storeAsXML;

        {
            juce::PropertiesFile props (file, options);
            props.setValue ("ui.monitorEnabled", true);

            const auto saveOk = props.save();
            expect (saveOk);
            expect (file.existsAsFile());
        }

        {
            juce::PropertiesFile reloaded (file, options);
            expect (reloaded.getBoolValue ("ui.monitorEnabled", false));
        }

        file.deleteFile();
    }
};

static SettingsPersistenceTests settingsPersistenceTests;

int main()
{
    juce::UnitTestRunner runner;
    runner.setPassesAreLogged (true);
    runner.runAllTests();

    int failures = 0;

    for (int i = 0; i < runner.getNumResults(); ++i)
        if (const auto* result = runner.getResult (i))
            failures += result->failures;

    return failures == 0 ? 0 : 1;
}
