#include <JuceHeader.h>

#include <cmath>

#include "api/ParameterRegistry.h"
#include "dsp/CabinetProcessor.h"
#include "dsp/ChainFactory.h"
#include "dsp/IrEngine.h"
#include "dsp/ReverbProcessor.h"
#include "preset/IrLibrary.h"

namespace
{
constexpr double kRate = 48000.0;

/**
 * Writes a bright impulse response: mostly a single spike.
 *
 * Deliberately different in character from writeTestIr's decaying burst, so a
 * blend between the two lands somewhere measurably in between rather than
 * being indistinguishable from either.
 */
bool writeBrightIr (const juce::File& file, int lengthSamples)
{
    file.getParentDirectory().createDirectory();
    file.deleteFile();

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());

    if (stream == nullptr)
        return false;

    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav.createWriterFor (stream.get(), kRate, 1, 16, {}, 0));

    if (writer == nullptr)
        return false;

    stream.release();

    juce::AudioBuffer<float> buffer (1, lengthSamples);
    buffer.clear();
    buffer.setSample (0, 0, 0.9f);
    buffer.setSample (0, lengthSamples / 2, -0.3f);

    writer->writeFromAudioSampleBuffer (buffer, 0, lengthSamples);
    writer.reset();

    return file.existsAsFile();
}

/** Writes a short, valid impulse response so the load path is exercised for real. */
bool writeTestIr (const juce::File& file, int lengthSamples, int numChannels = 1)
{
    file.getParentDirectory().createDirectory();
    file.deleteFile();

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());

    if (stream == nullptr)
        return false;

    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav.createWriterFor (stream.get(), kRate, (unsigned int) numChannels, 16, {}, 0));

    if (writer == nullptr)
        return false;

    stream.release(); // the writer owns it now

    juce::AudioBuffer<float> buffer (numChannels, lengthSamples);
    buffer.clear();

    // A decaying burst: enough content that convolving with it audibly changes
    // the signal, unlike a bare unit impulse.
    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < lengthSamples; ++i)
        {
            const auto decay = std::exp (-3.0f * (float) i / (float) lengthSamples);
            buffer.setSample (ch, i, decay * std::sin (0.05f * (float) i));
        }

    writer->writeFromAudioSampleBuffer (buffer, 0, lengthSamples);
    writer.reset();

    return file.existsAsFile();
}

float rmsOf (const juce::AudioBuffer<float>& buffer)
{
    double sum = 0.0;
    const auto n = buffer.getNumSamples() * buffer.getNumChannels();

    if (n <= 0)
        return 0.0f;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto s = (double) buffer.getSample (ch, i);
            sum += s * s;
        }

    return (float) std::sqrt (sum / (double) n);
}

void fillNoise (juce::AudioBuffer<float>& buffer, juce::Random& random)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample (ch, i, random.nextFloat() * 0.5f - 0.25f);
}

bool allFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (buffer.getSample (ch, i)))
                return false;

    return true;
}
} // namespace

//==============================================================================
class IrEngineTests final : public juce::UnitTest
{
public:
    IrEngineTests() : juce::UnitTest ("IrEngine") {}

    void runTest() override
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getNonexistentChildFile ("MilodikFXIrTest", "", false);
        dir.createDirectory();

        const auto irFile = dir.getChildFile ("test-cab.wav");
        expect (writeTestIr (irFile, 512), "could not write the test impulse response");

        beginTest ("Nothing loaded means the buffer is left alone");

        milodikfx::dsp::IrEngine engine;
        engine.prepare (kRate, 512, 2);

        expect (! engine.hasImpulseResponse());
        expect (engine.getLoadedName().isEmpty());

        juce::AudioBuffer<float> buffer (2, 512);
        juce::Random random (1234);
        fillNoise (buffer, random);

        juce::AudioBuffer<float> untouched (2, 512);
        untouched.makeCopyOf (buffer);

        expect (! engine.process (buffer), "process reported success with no IR loaded");

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            expect (std::abs (buffer.getSample (0, i) - untouched.getSample (0, i)) < 1.0e-9f);

        beginTest ("A real file loads and changes the signal");

        expect (engine.loadFromFile (irFile), "loading a valid WAV failed");
        expect (engine.hasImpulseResponse());
        expectEquals (engine.getLoadedName(), juce::String ("test-cab"));

        // Convolution runs its update on a background queue, so give it a moment
        // to swap the engine in before measuring.
        auto convolved = 0.0f;

        for (int attempt = 0; attempt < 40 && convolved < 1.0e-6f; ++attempt)
        {
            fillNoise (buffer, random);
            engine.process (buffer);
            convolved = rmsOf (buffer);

            if (convolved < 1.0e-6f)
                juce::Thread::sleep (25);
        }

        logMessage ("convolved rms " + juce::String (convolved));
        expect (convolved > 1.0e-5f, "convolution produced silence");
        expect (allFinite (buffer), "convolution produced a non-finite sample");

        beginTest ("A missing file clears the engine rather than keeping the old one");

        expect (! engine.loadFromFile (dir.getChildFile ("does-not-exist.wav")));
        expect (! engine.hasImpulseResponse(), "a failed load left the previous IR in place");
        expect (engine.getLoadedName().isEmpty());

        beginTest ("A file that is not audio is rejected");

        const auto junk = dir.getChildFile ("not-audio.wav");
        junk.replaceWithText ("this is definitely not a wav file");

        expect (! engine.loadFromFile (junk), "a text file was accepted as an impulse response");
        expect (! engine.hasImpulseResponse());

        beginTest ("Survives a sample-rate change while loaded");

        expect (engine.loadFromFile (irFile));

        for (const auto rate : { 44100.0, 96000.0, 48000.0 })
        {
            engine.prepare (rate, 512, 2);

            for (int block = 0; block < 8; ++block)
            {
                fillNoise (buffer, random);
                engine.process (buffer);
                expect (allFinite (buffer), "non-finite output after switching to " + juce::String (rate));
            }
        }

        dir.deleteRecursively();
    }
};

static IrEngineTests irEngineTests;

//==============================================================================
class IrLibraryTests final : public juce::UnitTest
{
public:
    IrLibraryTests() : juce::UnitTest ("IrLibrary") {}

    void runTest() override
    {
        auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getNonexistentChildFile ("MilodikFXIrLib", "", false);

        milodikfx::preset::IrLibrary library (root);
        using Category = milodikfx::preset::IrLibrary::Category;

        beginTest ("Categories get their own directories");

        expect (library.getDirectory (Category::cabinet).isDirectory());
        expect (library.getDirectory (Category::reverb).isDirectory());
        expect (library.getDirectory (Category::cabinet) != library.getDirectory (Category::reverb));

        beginTest ("Listing finds what was written and keeps categories apart");

        expect (writeTestIr (library.getDirectory (Category::cabinet).getChildFile ("Marshall 4x12.wav"), 256));
        expect (writeTestIr (library.getDirectory (Category::reverb).getChildFile ("Big Hall.wav"), 256));

        const auto cabinets = library.list (Category::cabinet);
        const auto reverbs = library.list (Category::reverb);

        expect (cabinets.contains ("Marshall 4x12"));
        expect (! cabinets.contains ("Big Hall"), "a reverb IR showed up in the cabinet list");
        expect (reverbs.contains ("Big Hall"));

        beginTest ("Resolving finds a known name and refuses an unknown one");

        expect (library.resolve (Category::cabinet, "Marshall 4x12").existsAsFile());
        expect (library.resolve (Category::cabinet, "marshall 4x12").existsAsFile(), "lookup was case sensitive");
        expect (! library.resolve (Category::cabinet, "Nope").existsAsFile());
        expect (! library.resolve (Category::cabinet, "").existsAsFile());

        beginTest ("A name cannot escape its directory");

        // The same class of defect that once let a preset name walk out of the
        // presets folder. Note that a hostile name is allowed to *succeed* --
        // it just has to end up as a plain filename inside the category
        // directory, which is what the checks below actually assert.
        const auto cabinetDir = library.getDirectory (Category::cabinet);

        for (const auto* attempt : { "../evil", "..\\evil", "C:/Windows/evil", "sub/dir/evil", ".." })
        {
            const auto name = juce::String (attempt);
            const auto sanitised = milodikfx::preset::IrLibrary::sanitiseName (name);

            expect (! sanitised.containsChar ('/'), "slash survived: " + sanitised);
            expect (! sanitised.containsChar ('\\'), "backslash survived: " + sanitised);
            expect (! sanitised.contains (".."), "traversal survived: " + sanitised);

            const auto resolved = library.resolve (Category::cabinet, name);

            if (resolved != juce::File())
                expect (resolved.isAChildOf (cabinetDir),
                        "a hostile name resolved outside the library: " + resolved.getFullPathName());

            juce::MemoryBlock data (64, true);
            library.import (Category::cabinet, name, data);
        }

        juce::Array<juce::File> escaped;
        root.getParentDirectory().findChildFiles (escaped, juce::File::findFiles, false, "evil*");
        expect (escaped.isEmpty(), "an import was written outside the library directory");

        // And nothing landed above the category folder either.
        juce::Array<juce::File> strays;
        root.findChildFiles (strays, juce::File::findFiles, false, "*");
        expect (strays.isEmpty(), "an import was written to the library root");

        beginTest ("Import stores the data and makes it listable");

        juce::MemoryBlock payload;
        {
            const auto source = library.getDirectory (Category::cabinet).getChildFile ("Marshall 4x12.wav");
            expect (source.loadFileAsData (payload));
        }

        const auto stored = library.import (Category::cabinet, "Imported Cab", payload);
        expectEquals (stored, juce::String ("Imported Cab"));
        expect (library.list (Category::cabinet).contains ("Imported Cab"));
        expect (library.resolve (Category::cabinet, "Imported Cab").existsAsFile());

        root.deleteRecursively();
    }
};

static IrLibraryTests irLibraryTests;

//==============================================================================
class IrChainIntegrationTests final : public juce::UnitTest
{
public:
    IrChainIntegrationTests() : juce::UnitTest ("IR chain integration") {}

    void runTest() override
    {
        auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getNonexistentChildFile ("MilodikFXIrChain", "", false);

        milodikfx::preset::IrLibrary library (root);
        using Category = milodikfx::preset::IrLibrary::Category;

        expect (writeTestIr (library.getDirectory (Category::cabinet).getChildFile ("Cab A.wav"), 512));

        milodikfx::dsp::DSPChainManager manager;
        const auto chain = milodikfx::dsp::buildGuitarChain (manager);

        milodikfx::dsp::ChainExtras extras;
        extras.irLibrary = &library;

        milodikfx::api::ParameterRegistry registry;
        milodikfx::dsp::registerChainParameters (registry, chain, manager, std::move (extras));

        beginTest ("Cabinet and reverb expose an IR choice when a library is supplied");

        const auto* cabinetIr = registry.findParameter ("cabinet", "irFile");
        expect (cabinetIr != nullptr, "cabinet has no irFile parameter");

        if (cabinetIr != nullptr)
        {
            expect (cabinetIr->isText, "irFile should be a text parameter");
            expect (cabinetIr->getOptions != nullptr, "irFile offers no options");
            expect (cabinetIr->getOptions().contains ("Cab A"), "the library file was not offered");
        }

        expect (registry.findParameter ("reverb", "irFile") != nullptr, "reverb has no irFile parameter");

        beginTest ("Selecting a known IR loads it, an unknown one clears it");

        juce::String applied;
        expect (registry.setTextParameter ("cabinet", "irFile", "Cab A", applied));
        expectEquals (applied, juce::String ("Cab A"));
        expect (chain.cabinet->getIrEngine().hasImpulseResponse());

        expect (registry.setTextParameter ("cabinet", "irFile", "Missing", applied));
        expect (applied.isEmpty(), "an unknown IR name was reported as loaded");
        expect (! chain.cabinet->getIrEngine().hasImpulseResponse(),
                "an unknown name left the previous IR loaded");

        beginTest ("A numeric write to a text parameter is refused");

        float ignored = 0.0f;
        expect (! registry.setParameter ("cabinet", "irFile", 1.0f, ignored),
                "irFile accepted a numeric write");

        beginTest ("IR selection survives a preset round trip");

        expect (registry.setTextParameter ("cabinet", "irFile", "Cab A", applied));

        const auto snapshot = registry.captureState();

        registry.setTextParameter ("cabinet", "irFile", "", applied);
        expect (! chain.cabinet->getIrEngine().hasImpulseResponse());

        expect (registry.applyState (snapshot) > 0);
        expectEquals (chain.cabinet->getIrEngine().getLoadedName(), juce::String ("Cab A"),
                      "the IR choice did not survive capture/apply");

        beginTest ("Cabinet falls back to the analytic chain when no IR is loaded");

        // With IR mode on but nothing loaded, the analytic filters must still
        // run: losing the file must never mean losing the sound.
        chain.cabinet->setEnabled (true);
        chain.cabinet->setUseImpulseResponse (true);
        registry.setTextParameter ("cabinet", "irFile", "", applied);

        manager.prepareToPlay (kRate, 512, 2);

        juce::AudioBuffer<float> buffer (2, 512);
        juce::Random random (99);
        fillNoise (buffer, random);

        chain.cabinet->processBlock (buffer);

        expect (rmsOf (buffer) > 1.0e-4f, "cabinet went silent when the IR was missing");
        expect (allFinite (buffer));

        root.deleteRecursively();
    }
};

static IrChainIntegrationTests irChainIntegrationTests;

//==============================================================================
class DualIrTests final : public juce::UnitTest
{
public:
    DualIrTests() : juce::UnitTest ("Dual IR blend", "dsp") {}

    void runTest() override
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("MilodikFX_DualIrTests");
        dir.deleteRecursively();
        dir.createDirectory();

        const auto darkFile = dir.getChildFile ("dark.wav");
        const auto brightFile = dir.getChildFile ("bright.wav");

        expect (writeTestIr (darkFile, 512));
        expect (writeBrightIr (brightFile, 512));

        auto runCabinet = [&] (bool loadA, bool loadB, float blend)
        {
            milodikfx::dsp::CabinetProcessor cab;
            cab.setEnabled (true);
            cab.setUseImpulseResponse (true);
            // Prepared for the block it is actually given: the convolution
            // allocates its working buffers from this figure.
            cab.prepareToPlay (kRate, 2048, 1);

            if (loadA)
                expect (cab.getIrEngine().loadFromFile (darkFile));

            if (loadB)
                expect (cab.getIrEngineB().loadFromFile (brightFile));

            cab.setIrBlend (blend);

            // juce::dsp::Convolution loads on a background thread, so the first
            // blocks after a load still run the previous (empty) response. Wait
            // it out, then warm up, or the comparison is a race.
            juce::Thread::sleep (300);

            juce::AudioBuffer<float> buffer (1, 2048);

            for (int pass = 0; pass < 8; ++pass)
            {
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    buffer.setSample (0, i, (float) std::sin (0.03 * (double) i) * 0.4f);

                cab.processBlock (buffer);
            }

            return buffer;
        };

        beginTest ("Blend at either end is exactly that IR alone");
        {
            const auto onlyA = runCabinet (true, false, 0.0f);
            const auto bothAtA = runCabinet (true, true, 0.0f);

            for (int i = 0; i < onlyA.getNumSamples(); ++i)
                expectWithinAbsoluteError (bothAtA.getSample (0, i), onlyA.getSample (0, i), 1.0e-5f,
                                           "loading a second IR changed the sound at blend 0");

            const auto onlyB = runCabinet (false, true, 1.0f);
            const auto bothAtB = runCabinet (true, true, 1.0f);

            for (int i = 0; i < onlyB.getNumSamples(); ++i)
                expectWithinAbsoluteError (bothAtB.getSample (0, i), onlyB.getSample (0, i), 1.0e-5f,
                                           "blend 1 was not the second IR alone");
        }

        beginTest ("A blend lands between the two, not on either");
        {
            const auto a = runCabinet (true, true, 0.0f);
            const auto b = runCabinet (true, true, 1.0f);
            const auto mixed = runCabinet (true, true, 0.5f);

            auto differsFrom = [] (const juce::AudioBuffer<float>& x,
                                   const juce::AudioBuffer<float>& y)
            {
                double sum = 0.0;

                for (int i = 0; i < x.getNumSamples(); ++i)
                {
                    const auto d = (double) x.getSample (0, i) - (double) y.getSample (0, i);
                    sum += d * d;
                }

                return std::sqrt (sum / (double) x.getNumSamples());
            };

            logMessage ("  rms A " + juce::String (rmsOf (a), 5)
                        + "  B " + juce::String (rmsOf (b), 5)
                        + "  mixed " + juce::String (rmsOf (mixed), 5));

            expect (differsFrom (mixed, a) > 1.0e-4, "the blend sounded exactly like IR A");
            expect (differsFrom (mixed, b) > 1.0e-4, "the blend sounded exactly like IR B");

            // And it really is the average of the two, sample for sample.
            for (int i = 0; i < mixed.getNumSamples(); ++i)
                expectWithinAbsoluteError (mixed.getSample (0, i),
                                           0.5f * (a.getSample (0, i) + b.getSample (0, i)),
                                           1.0e-4f);
        }

        beginTest ("One IR missing still works rather than going silent");
        {
            // Whatever is loaded should be heard. Falling silent because the
            // other slot is empty would be the worst possible failure here.
            const auto onlyB = runCabinet (false, true, 0.0f);
            expect (onlyB.getMagnitude (0, onlyB.getNumSamples()) > 0.001f,
                    "blend 0 with only B loaded produced silence");

            const auto onlyA = runCabinet (true, false, 1.0f);
            expect (onlyA.getMagnitude (0, onlyA.getNumSamples()) > 0.001f,
                    "blend 1 with only A loaded produced silence");
        }

        beginTest ("Neither loaded falls back to the analytic cabinet");
        {
            const auto neither = runCabinet (false, false, 0.5f);

            expect (neither.getMagnitude (0, neither.getNumSamples()) > 0.001f,
                    "no IR at all produced silence instead of the analytic chain");
        }

        beginTest ("The blend is clamped to its range");
        {
            milodikfx::dsp::CabinetProcessor cab;

            cab.setIrBlend (-4.0f);
            expectEquals (cab.getIrBlend(), 0.0f);

            cab.setIrBlend (9.0f);
            expectEquals (cab.getIrBlend(), 1.0f);
        }

        dir.deleteRecursively();
    }
};

static DualIrTests dualIrTests;
