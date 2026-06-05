#include <JuceHeader.h>

#include <cmath>

#include "dsp/DSPChainManager.h"

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

namespace
{
class AddConstantProcessor final : public milodikfx::dsp::AudioProcessorBase
{
public:
    explicit AddConstantProcessor (float offsetIn)
        : offset (offsetIn)
    {
    }

    void prepareToPlay (double, int, int) override
    {
        prepared = true;
    }

    void processBlock (juce::AudioBuffer<float>& buffer) override
    {
        if (! prepared)
            return;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (ch, i, buffer.getSample (ch, i) + offset);
    }

    void reset() override {}

private:
    float offset = 0.0f;
    bool prepared = false;
};

class MultiplyProcessor final : public milodikfx::dsp::AudioProcessorBase
{
public:
    explicit MultiplyProcessor (float gainIn)
        : gain (gainIn)
    {
    }

    void prepareToPlay (double, int, int) override
    {
        prepared = true;
    }

    void processBlock (juce::AudioBuffer<float>& buffer) override
    {
        if (! prepared)
            return;

        buffer.applyGain (gain);
    }

    void reset() override {}

private:
    float gain = 1.0f;
    bool prepared = false;
};
} // namespace

class DSPChainManagerTests final : public juce::UnitTest
{
public:
    DSPChainManagerTests()
        : juce::UnitTest ("DSPChainManager")
    {
    }

    void runTest() override
    {
        beginTest ("Processors run in order");

        milodikfx::dsp::DSPChainManager chain;
        chain.addProcessor (std::make_unique<AddConstantProcessor> (1.0f));
        chain.addProcessor (std::make_unique<MultiplyProcessor> (2.0f));
        chain.prepareToPlay (48000.0, 32, 1);

        juce::AudioBuffer<float> buffer (1, 4);
        buffer.clear();

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample (0, i, 1.0f);

        chain.processBlock (buffer);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            expect (std::abs (buffer.getSample (0, i) - 4.0f) < 0.0001f);

        beginTest ("Bypass keeps signal intact");

        chain.setBypassed (true);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample (0, i, 0.5f);

        chain.processBlock (buffer);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            expect (std::abs (buffer.getSample (0, i) - 0.5f) < 0.0001f);
    }
};

static DSPChainManagerTests dspChainManagerTests;

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
