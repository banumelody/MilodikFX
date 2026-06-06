#include <JuceHeader.h>

#include "MainComponent.h"

static constexpr const char* kKeyWindowBounds = "ui.windowBounds";

class MilodikFXApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "MilodikFX"; }
    const juce::String getApplicationVersion() override { return ProjectInfo::versionString; }

    void initialise (const juce::String&) override
    {
        juce::PropertiesFile::Options options;
        options.applicationName = getApplicationName();
        options.osxLibrarySubFolder = "Application Support";
        options.millisecondsBeforeSaving = -1; // explicit save in MainComponent
        options.storageFormat = juce::PropertiesFile::storeAsXML;

        auto base = juce::SystemStats::getEnvironmentVariable ("APPDATA", {});
        if (base.isEmpty())
            base = juce::SystemStats::getEnvironmentVariable ("LOCALAPPDATA", {});

        auto dir = base.isNotEmpty()
            ? juce::File (base).getChildFile ("MilodikFX")
            : juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("MilodikFX");

        dir.createDirectory();

        auto file = dir.getChildFile ("MilodikFX.settings");
        settingsFile = std::make_unique<juce::PropertiesFile> (file, options);

        mainWindow = std::make_unique<MainWindow> (getApplicationName(), *settingsFile);
    }

    void shutdown() override
    {
        mainWindow.reset();

        if (settingsFile != nullptr)
            settingsFile->save();

        settingsFile.reset();
    }

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        juce::PropertiesFile& settingsFile;

        MainWindow(juce::String name, juce::PropertiesFile& settings)
            : juce::DocumentWindow(std::move(name),
                                   juce::Colours::darkgrey,
                                   juce::DocumentWindow::allButtons),
              settingsFile(settings)
        {
            setUsingNativeTitleBar(true);
            setResizable(false, false);
            setContentOwned(new MainComponent(settingsFile), true);

            // Minimize window (hidden from user - only backend runs)
            setBounds(0, 0, 1, 1);
            setVisible(false);

            // Launch browser to React UI
            juce::URL("http://localhost:3000").launchInDefaultBrowser();
        }

        void closeButtonPressed() override
        {
            settingsFile.save();
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<juce::PropertiesFile> settingsFile;

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (MilodikFXApplication)
