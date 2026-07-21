#include <JuceHeader.h>

#include <fstream>

#include "MainComponent.h"

#if JUCE_DEBUG && JUCE_WINDOWS
 #include <crtdbg.h>
#endif

#if JUCE_WINDOWS
 #include <dwmapi.h>
 #pragma comment (lib, "dwmapi.lib")
#endif

namespace
{
constexpr const char* kKeyWindowBounds = "ui.windowBounds";
constexpr const char* kKeyWindowMaximised = "ui.windowMaximised";

/** Log to %APPDATA%, not next to the exe, which may sit under Program Files. */
juce::File getAppDataDirectory()
{
    auto base = juce::SystemStats::getEnvironmentVariable ("APPDATA", {});

    if (base.isEmpty())
        base = juce::SystemStats::getEnvironmentVariable ("LOCALAPPDATA", {});

    auto dir = base.isNotEmpty()
                   ? juce::File (base).getChildFile ("MilodikFX")
                   : juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                         .getChildFile ("MilodikFX");

    dir.createDirectory();
    return dir;
}

/**
 * Appending file logger that rolls over once the file gets large, so a long
 * running session cannot fill the disk.
 */
class AppFileLogger final : public juce::Logger
{
public:
    explicit AppFileLogger (const juce::File& file)
        : logFile (file)
    {
        if (logFile.existsAsFile() && logFile.getSize() > kMaxBytes)
        {
            auto previous = logFile.getSiblingFile (logFile.getFileNameWithoutExtension() + ".1.log");
            previous.deleteFile();
            logFile.moveFileTo (previous);
        }

        stream.open (logFile.getFullPathName().toStdString(), std::ios::app);
    }

    void logMessage (const juce::String& message) override
    {
        const juce::ScopedLock lock (mutex);

        if (! stream.is_open())
            return;

        stream << juce::Time::getCurrentTime().toISO8601 (true).toStdString()
               << "  " << message.toStdString() << "\n";
        stream.flush();
    }

private:
    static constexpr juce::int64 kMaxBytes = 4 * 1024 * 1024;

    juce::File logFile;
    std::ofstream stream;
    juce::CriticalSection mutex;
};

#if JUCE_DEBUG && JUCE_WINDOWS
/**
 * Turns a Debug CRT assertion (an STL bounds check, say) into a logged stack
 * trace instead of a modal dialog.
 *
 * A dialog stops the process dead with nothing written down, which is exactly
 * what happened when a "vector subscript out of range" showed up during a
 * device change: no trace, no clue which container it was.
 */
int crtReportHook (int reportType, char* message, int* returnValue)
{
    if (reportType == _CRT_ASSERT || reportType == _CRT_ERROR)
    {
        if (auto* logger = juce::Logger::getCurrentLogger())
        {
            logger->writeToLog ("!!! CRT ASSERTION: " + juce::String (message != nullptr ? message : "(none)"));
            logger->writeToLog ("--- stack ---\n" + juce::SystemStats::getStackBacktrace());
        }
    }

    if (returnValue != nullptr)
        *returnValue = 0; // carry on rather than break into a debugger

    return TRUE; // handled: suppress the dialog
}
#endif

/** Renders the tray/window icon so no external asset is needed at runtime. */
juce::Image createAppIcon (int size)
{
    juce::Image image (juce::Image::ARGB, size, size, true);
    juce::Graphics g (image);

    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) size, (float) size).reduced (size * 0.06f);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b6cff), bounds.getTopLeft(),
                                             juce::Colour (0xff7b2bff), bounds.getBottomRight(), false));
    g.fillRoundedRectangle (bounds, size * 0.22f);

    g.setColour (juce::Colours::white.withAlpha (0.95f));

    // A stylised waveform, drawn rather than shipped as a file.
    juce::Path wave;
    const auto midY = bounds.getCentreY();
    const auto left = bounds.getX() + bounds.getWidth() * 0.16f;
    const auto right = bounds.getRight() - bounds.getWidth() * 0.16f;
    const auto amp = bounds.getHeight() * 0.26f;
    const auto steps = 48;

    for (int i = 0; i <= steps; ++i)
    {
        const auto t = (float) i / (float) steps;
        const auto x = left + (right - left) * t;
        const auto y = midY - amp * std::sin (t * juce::MathConstants<float>::twoPi)
                              * (0.45f + 0.55f * std::sin (t * juce::MathConstants<float>::pi));

        if (i == 0)
            wave.startNewSubPath (x, y);
        else
            wave.lineTo (x, y);
    }

    g.strokePath (wave, juce::PathStrokeType (juce::jmax (1.5f, size * 0.075f),
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    return image;
}
} // namespace

class MilodikFXApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "MilodikFX"; }
    const juce::String getApplicationVersion() override { return ProjectInfo::versionString; }

    // A second engine fighting over the same interface and port helps nobody.
    bool moreThanOneInstanceAllowed() override { return false; }

    void anotherInstanceStarted (const juce::String&) override
    {
        if (mainWindow != nullptr)
            mainWindow->bringToFrontAndRestore();
    }

    void initialise (const juce::String&) override
    {
        const auto appData = getAppDataDirectory();

        juce::Logger::setCurrentLogger (new AppFileLogger (appData.getChildFile ("milodikfx.log")));

       #if JUCE_DEBUG && JUCE_WINDOWS
        _CrtSetReportHook (crtReportHook);
       #endif

        juce::Logger::getCurrentLogger()->writeToLog ("========== MilodikFX "
                                                     + getApplicationVersion() + " started ==========");
        juce::Logger::getCurrentLogger()->writeToLog (
            "Executable: " + juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName());

        juce::PropertiesFile::Options options;
        options.applicationName = getApplicationName();
        options.osxLibrarySubFolder = "Application Support";
        options.millisecondsBeforeSaving = -1; // saved explicitly by MainComponent
        options.storageFormat = juce::PropertiesFile::storeAsXML;

        settingsFile = std::make_unique<juce::PropertiesFile> (appData.getChildFile ("MilodikFX.settings"), options);

        mainWindow = std::make_unique<MainWindow> (getApplicationName(), *settingsFile);

        juce::Logger::getCurrentLogger()->writeToLog ("Main window created");
    }

    void shutdown() override
    {
        mainWindow.reset();

        if (settingsFile != nullptr)
            settingsFile->save();

        settingsFile.reset();

        juce::Logger::getCurrentLogger()->writeToLog ("========== MilodikFX stopped ==========");
        juce::Logger::setCurrentLogger (nullptr);
    }

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr)
            mainWindow->prepareToQuit();

        quit();
    }

private:
    class MainWindow;

    /** Keeps the app reachable after the window is closed to the tray. */
    class TrayIcon final : public juce::SystemTrayIconComponent
    {
    public:
        explicit TrayIcon (MainWindow& ownerWindow)
            : owner (ownerWindow)
        {
            setIconImage (createAppIcon (64), createAppIcon (32));
            setIconTooltip ("MilodikFX");
        }

        void mouseDown (const juce::MouseEvent& event) override;

    private:
        MainWindow& owner;
    };

    class MainWindow final : public juce::DocumentWindow
    {
    public:
        MainWindow (juce::String name, juce::PropertiesFile& settings)
            : juce::DocumentWindow (std::move (name),
                                    juce::Colour (0xff0d0f14),
                                    juce::DocumentWindow::allButtons),
              settingsFile (settings)
        {
            setUsingNativeTitleBar (true);
            setResizable (true, false);
            setResizeLimits (900, 600, 4000, 3000);

            auto* content = new MainComponent (settingsFile);
            component = content;
            setContentOwned (content, true);

            setIcon (createAppIcon (256));

            restoreBounds();
            setVisible (true);
            toFront (true);

            // Only once the peer exists, which needs the window to be visible.
            applyDarkTitleBar();

            trayIcon = std::make_unique<TrayIcon> (*this);
        }

        ~MainWindow() override
        {
            trayIcon.reset();
        }

        void prepareToQuit()
        {
            storeBounds();

            if (component != nullptr)
                component->flushSettings();

            settingsFile.save();
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

        void bringToFrontAndRestore()
        {
            setVisible (true);
            setMinimised (false);
            toFront (true);
        }

    private:
        /**
         * Asks Windows for the dark title bar.
         *
         * The UI behind it is near-black; the default light frame around it
         * reads as a rendering bug. DWMWA_USE_IMMERSIVE_DARK_MODE is 20 on
         * Windows 10 20H1 and later and 19 on the builds just before it, so both
         * are tried -- an unsupported attribute is simply refused, which is why
         * the return value is ignored.
         */
        void applyDarkTitleBar()
        {
           #if JUCE_WINDOWS
            auto* peer = getPeer();

            if (peer == nullptr)
                return;

            auto* handle = (HWND) peer->getNativeHandle();

            if (handle == nullptr)
                return;

            const BOOL dark = TRUE;

            for (const DWORD attribute : { (DWORD) 20, (DWORD) 19 })
                DwmSetWindowAttribute (handle, attribute, &dark, sizeof (dark));
           #endif
        }

        void restoreBounds()
        {
            const auto stored = settingsFile.getValue (kKeyWindowBounds, {});

            if (stored.isNotEmpty())
            {
                const auto bounds = juce::Rectangle<int>::fromString (stored);

                // Ignore a saved position that lands off every current display,
                // which happens after unplugging a second monitor.
                if (! bounds.isEmpty()
                    && juce::Desktop::getInstance().getDisplays().getDisplayForRect (bounds) != nullptr)
                {
                    setBounds (bounds);

                    if (settingsFile.getBoolValue (kKeyWindowMaximised, false))
                        setFullScreen (true);

                    return;
                }
            }

            centreWithSize (1180, 760);
        }

        void storeBounds()
        {
            settingsFile.setValue (kKeyWindowMaximised, isFullScreen());

            if (! isFullScreen())
                settingsFile.setValue (kKeyWindowBounds, getBounds().toString());
        }

        juce::PropertiesFile& settingsFile;
        juce::Component::SafePointer<MainComponent> component;
        std::unique_ptr<TrayIcon> trayIcon;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    std::unique_ptr<juce::PropertiesFile> settingsFile;
    std::unique_ptr<MainWindow> mainWindow;
};

void MilodikFXApplication::TrayIcon::mouseDown (const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Show MilodikFX");
        menu.addSeparator();
        menu.addItem (2, "Quit");

        menu.showMenuAsync (juce::PopupMenu::Options(), [this] (int result)
        {
            if (result == 1)
                owner.bringToFrontAndRestore();
            else if (result == 2)
                juce::JUCEApplication::getInstance()->systemRequestedQuit();
        });

        return;
    }

    owner.bringToFrontAndRestore();
}

START_JUCE_APPLICATION (MilodikFXApplication)
