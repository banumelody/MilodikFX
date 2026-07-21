#include <JuceHeader.h>

#include <cmath>
#include <iostream>

#include "dsp/DSPChainManager.h"
#include "dsp/GainProcessor.h"
#include "dsp/OverdriveProcessor.h"
#include "dsp/EQProcessor.h"

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

// PresetManager and ParameterRegistry are covered in RegistryTests.cpp.

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

        beginTest ("Bypass keeps signal intact once the crossfade has settled");

        chain.setBypassed (true);

        // Entering bypass crossfades over ~10 ms rather than switching hard, so
        // let the fade finish before checking the signal is untouched.
        juce::AudioBuffer<float> fadeBuffer (1, 1024);

        for (int block = 0; block < 4; ++block)
        {
            for (int i = 0; i < fadeBuffer.getNumSamples(); ++i)
                fadeBuffer.setSample (0, i, 0.5f);

            chain.processBlock (fadeBuffer);
        }

        for (int i = 0; i < fadeBuffer.getNumSamples(); ++i)
            expect (std::abs (fadeBuffer.getSample (0, i) - 0.5f) < 0.0001f);

        beginTest ("Leaving bypass does not step");

        chain.setBypassed (false);

        for (int i = 0; i < fadeBuffer.getNumSamples(); ++i)
            fadeBuffer.setSample (0, i, 0.5f);

        chain.processBlock (fadeBuffer);

        // First sample must still be close to the dry level; a hard switch would
        // jump straight to the processed value (0.5 + 1) * 2 = 3.0.
        expect (std::abs (fadeBuffer.getSample (0, 0) - 0.5f) < 0.05f,
                "bypass exit stepped: " + juce::String (fadeBuffer.getSample (0, 0)));

        chain.setBypassed (true);
        for (int block = 0; block < 4; ++block)
        {
            fadeBuffer.clear();
            chain.processBlock (fadeBuffer);
        }
        chain.setBypassed (false);
        for (int block = 0; block < 4; ++block)
        {
            fadeBuffer.clear();
            chain.processBlock (fadeBuffer);
        }

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

        // The level control is smoothed, so compare the settled tail of each run
        // rather than the first sample after the change.
        const auto out100 = measureDrivenLevel (100.0f);
        const auto out50 = measureDrivenLevel (50.0f);

        expect (out100 > 0.01f);
        expect (std::abs (out50 - (out100 * 0.5f)) < out100 * 0.02f);
        expect (out50 <= 1.0f);

        beginTest ("Level changes glide instead of stepping");

        milodikfx::dsp::OverdriveProcessor odGlide;
        odGlide.setEnabled (true);
        odGlide.setDrivePercent (0.0f);
        odGlide.setLevelPercent (100.0f);
        odGlide.prepareToPlay (48000.0, 512, 1);

        juce::AudioBuffer<float> glideBuf (1, 512);
        for (int i = 0; i < glideBuf.getNumSamples(); ++i)
            glideBuf.setSample (0, i, 0.5f);

        odGlide.processBlock (glideBuf);
        const auto lastBefore = glideBuf.getSample (0, glideBuf.getNumSamples() - 1);

        odGlide.setLevelPercent (0.0f);
        for (int i = 0; i < glideBuf.getNumSamples(); ++i)
            glideBuf.setSample (0, i, 0.5f);

        odGlide.processBlock (glideBuf);
        const auto firstAfter = glideBuf.getSample (0, 0);

        expect (std::abs (firstAfter - lastBefore) < 0.05f,
                "level stepped by " + juce::String (std::abs (firstAfter - lastBefore)));
    }

private:
    // Runs a long block at a fixed level and reports the peak of the settled
    // second half, so the smoother has finished gliding before we measure.
    static float measureDrivenLevel (float levelPercent)
    {
        milodikfx::dsp::OverdriveProcessor od;
        od.setEnabled (true);
        od.setDrivePercent (100.0f);
        od.setLevelPercent (levelPercent);
        od.prepareToPlay (48000.0, 4096, 1);

        juce::AudioBuffer<float> buffer (1, 4096);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample (0, i, 0.5f);

        od.processBlock (buffer);

        return buffer.getMagnitude (2048, 2048);
    }
};

static OverdriveProcessorTests overdriveProcessorTests;

namespace
{
static void fillSine (juce::AudioBuffer<float>& buffer, double sampleRate, double freqHz, float amplitude)
{
    const auto twoPi = juce::MathConstants<double>::twoPi;
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        const auto s = (float) (amplitude * std::sin (twoPi * freqHz * (double) i / sampleRate));
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, i, s);
    }
}

static float rms (const juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    double sumSq = 0.0;
    const auto denom = juce::jmax (1, numSamples * numChannels);

    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < numSamples; ++i)
        {
            const auto s = (double) buffer.getSample (ch, i);
            sumSq += s * s;
        }

    return (float) std::sqrt (sumSq / (double) denom);
}
} // namespace

class EQProcessorTests final : public juce::UnitTest
{
public:
    EQProcessorTests()
        : juce::UnitTest ("EQProcessor")
    {
    }

    void runTest() override
    {
        beginTest ("Disabled is passthrough");

        milodikfx::dsp::EQProcessor eq;
        eq.setEnabled (false);
        eq.setBassDb (12.0f);
        eq.setMidDb (-12.0f);
        eq.setTrebleDb (12.0f);
        eq.prepareToPlay (48000.0, 512, 1);

        juce::AudioBuffer<float> buffer (1, 512);
        fillSine (buffer, 48000.0, 1000.0, 0.2f);

        juce::AudioBuffer<float> original (buffer.getNumChannels(), buffer.getNumSamples());
        original.makeCopyOf (buffer);

        eq.processBlock (buffer);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            expect (std::abs (buffer.getSample (0, i) - original.getSample (0, i)) < 0.000001f);

        beginTest ("Mid +12dB boosts ~1kHz");

        milodikfx::dsp::EQProcessor eqMid;
        eqMid.setEnabled (true);
        eqMid.setBassDb (0.0f);
        eqMid.setMidDb (12.0f);
        eqMid.setTrebleDb (0.0f);
        eqMid.prepareToPlay (48000.0, 4096, 1);

        juce::AudioBuffer<float> midBuf (1, 4096);
        fillSine (midBuf, 48000.0, 1000.0, 0.2f);
        const auto beforeMid = rms (midBuf);

        eqMid.processBlock (midBuf);
        const auto afterMid = rms (midBuf);

        expect (afterMid > beforeMid * 2.5f);

        beginTest ("Bass +12dB boosts low frequencies");

        milodikfx::dsp::EQProcessor eqBass;
        eqBass.setEnabled (true);
        eqBass.setBassDb (12.0f);
        eqBass.setMidDb (0.0f);
        eqBass.setTrebleDb (0.0f);
        eqBass.prepareToPlay (48000.0, 4096, 1);

        juce::AudioBuffer<float> bassBuf (1, 4096);
        fillSine (bassBuf, 48000.0, 60.0, 0.2f);
        const auto beforeBass = rms (bassBuf);

        eqBass.processBlock (bassBuf);
        const auto afterBass = rms (bassBuf);

        expect (afterBass > beforeBass * 1.4f);

        beginTest ("Treble +12dB boosts high frequencies");

        milodikfx::dsp::EQProcessor eqTreble;
        eqTreble.setEnabled (true);
        eqTreble.setBassDb (0.0f);
        eqTreble.setMidDb (0.0f);
        eqTreble.setTrebleDb (12.0f);
        eqTreble.prepareToPlay (48000.0, 4096, 1);

        juce::AudioBuffer<float> trebleBuf (1, 4096);
        fillSine (trebleBuf, 48000.0, 10000.0, 0.15f);
        const auto beforeTreble = rms (trebleBuf);

        eqTreble.processBlock (trebleBuf);
        const auto afterTreble = rms (trebleBuf);

        expect (afterTreble > beforeTreble * 1.3f);
    }
};

static EQProcessorTests eqProcessorTests;

namespace
{
// JUCE's default logger goes to the debugger on Windows, which means a console
// run shows nothing at all. Route everything to stdout so ctest --output-on-failure
// and a direct run both show which test failed and why.
class ConsoleLogger final : public juce::Logger
{
public:
    void logMessage (const juce::String& message) override
    {
        std::cout << message << std::endl;
    }
};
} // namespace

int main()
{
    ConsoleLogger logger;
    juce::Logger::setCurrentLogger (&logger);

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.setPassesAreLogged (false);
    runner.runAllTests();

    int failures = 0;
    int passes = 0;

    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        if (const auto* result = runner.getResult (i))
        {
            failures += result->failures;
            passes += result->passes;

            if (result->failures > 0)
                std::cout << "FAILED: " << result->unitTestName << " / " << result->subcategoryName
                          << " (" << result->failures << " failures)" << std::endl;
        }
    }

    std::cout << "==== " << passes << " passed, " << failures << " failed ====" << std::endl;

    juce::Logger::setCurrentLogger (nullptr);

    return failures == 0 ? 0 : 1;
}
