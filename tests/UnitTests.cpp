#include <JuceHeader.h>

#include <cmath>

#include "dsp/DSPChainManager.h"
#include "dsp/GainProcessor.h"
#include "dsp/OverdriveProcessor.h"

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

        beginTest ("Gain processor applies dB gain");

        milodikfx::dsp::GainProcessor gain;
        gain.setEnabled (true);
        gain.setGainDb (6.0f);
        gain.prepareToPlay (48000.0, 32, 1);

        juce::AudioBuffer<float> gainBuffer (1, 4);
        gainBuffer.clear();
        for (int i = 0; i < gainBuffer.getNumSamples(); ++i)
            gainBuffer.setSample (0, i, 1.0f);

        gain.processBlock (gainBuffer);

        const auto expected = juce::Decibels::decibelsToGain (6.0f);
        for (int i = 0; i < gainBuffer.getNumSamples(); ++i)
            expect (std::abs (gainBuffer.getSample (0, i) - expected) < 0.0001f);

        beginTest ("Gain processor bypasses when disabled");

        gain.setEnabled (false);
        for (int i = 0; i < gainBuffer.getNumSamples(); ++i)
            gainBuffer.setSample (0, i, 0.25f);

        gain.processBlock (gainBuffer);

        for (int i = 0; i < gainBuffer.getNumSamples(); ++i)
            expect (std::abs (gainBuffer.getSample (0, i) - 0.25f) < 0.0001f);
    }
};

static DSPChainManagerTests dspChainManagerTests;

class OverdriveProcessorTests final : public juce::UnitTest
{
public:
    OverdriveProcessorTests()
        : juce::UnitTest ("OverdriveProcessor")
    {
    }

    void runTest() override
    {
        beginTest ("Drive 0, Level 100 is clean");

        milodikfx::dsp::OverdriveProcessor od;
        od.setEnabled (true);
        od.setDrivePercent (0.0f);
        od.setLevelPercent (100.0f);
        od.prepareToPlay (48000.0, 32, 1);

        juce::AudioBuffer<float> buffer (1, 4);
        buffer.setSample (0, 0, 0.25f);
        buffer.setSample (0, 1, -0.5f);
        buffer.setSample (0, 2, 0.0f);
        buffer.setSample (0, 3, 0.9f);

        od.processBlock (buffer);

        expect (std::abs (buffer.getSample (0, 0) - 0.25f) < 0.000001f);
        expect (std::abs (buffer.getSample (0, 1) - (-0.5f)) < 0.000001f);
        expect (std::abs (buffer.getSample (0, 2) - 0.0f) < 0.000001f);
        expect (std::abs (buffer.getSample (0, 3) - 0.9f) < 0.000001f);

        beginTest ("Disabled is passthrough");

        milodikfx::dsp::OverdriveProcessor odDisabled;
        odDisabled.setEnabled (false);
        odDisabled.setDrivePercent (100.0f);
        odDisabled.setLevelPercent (0.0f);
        odDisabled.prepareToPlay (48000.0, 32, 1);

        juce::AudioBuffer<float> buffer2 (1, 4);
        buffer2.clear();
        for (int i = 0; i < buffer2.getNumSamples(); ++i)
            buffer2.setSample (0, i, 0.2f);

        odDisabled.processBlock (buffer2);

        for (int i = 0; i < buffer2.getNumSamples(); ++i)
            expect (std::abs (buffer2.getSample (0, i) - 0.2f) < 0.000001f);

        beginTest ("Level scales output after drive");

        milodikfx::dsp::OverdriveProcessor odLevel;
        odLevel.setEnabled (true);
        odLevel.setDrivePercent (100.0f);
        odLevel.setLevelPercent (100.0f);
        odLevel.prepareToPlay (48000.0, 32, 1);

        juce::AudioBuffer<float> buffer3 (1, 4);
        for (int i = 0; i < buffer3.getNumSamples(); ++i)
            buffer3.setSample (0, i, 0.5f);

        odLevel.processBlock (buffer3);
        const auto out100 = buffer3.getSample (0, 0);

        // Re-run with level 50% on the same input.
        odLevel.setLevelPercent (50.0f);
        for (int i = 0; i < buffer3.getNumSamples(); ++i)
            buffer3.setSample (0, i, 0.5f);

        odLevel.processBlock (buffer3);
        const auto out50 = buffer3.getSample (0, 0);

        expect (std::abs (out50 - (out100 * 0.5f)) < 0.0001f);
        expect (std::abs (out50) <= 1.0f);
    }
};

static OverdriveProcessorTests overdriveProcessorTests;

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
