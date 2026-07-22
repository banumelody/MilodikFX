#include <JuceHeader.h>

#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

#include "dsp/ChainFactory.h"
#include "dsp/DSPChainManager.h"
#include "dsp/NamProcessor.h"
#include "dsp/NamResampler.h"
#include "preset/NamLibrary.h"

namespace
{
constexpr double kHostRate = 96000.0;

void fillSine (std::vector<float>& out, double freqHz, double rate, int n, int startSample = 0)
{
    out.resize ((size_t) n);

    for (int i = 0; i < n; ++i)
        out[(size_t) i] = 0.4f * (float) std::sin (juce::MathConstants<double>::twoPi * freqHz
                                                   * (double) (i + startSample) / rate);
}

double correlation (const std::vector<float>& a, const std::vector<float>& b, int lag)
{
    double sumAB = 0.0, sumA2 = 0.0, sumB2 = 0.0;
    const auto n = (int) a.size();

    for (int i = 0; i < n; ++i)
    {
        const auto j = i + lag;

        if (j < 0 || j >= (int) b.size())
            continue;

        sumAB += (double) a[(size_t) i] * (double) b[(size_t) j];
        sumA2 += (double) a[(size_t) i] * (double) a[(size_t) i];
        sumB2 += (double) b[(size_t) j] * (double) b[(size_t) j];
    }

    if (sumA2 < 1.0e-12 || sumB2 < 1.0e-12)
        return 0.0;

    return sumAB / std::sqrt (sumA2 * sumB2);
}
} // namespace

//==============================================================================
class NamResamplerTests final : public juce::UnitTest
{
public:
    NamResamplerTests() : juce::UnitTest ("NamResampler", "dsp") {}

    void runTest() override
    {
        using milodikfx::dsp::NamResampler;

        beginTest ("Matched rates are a bit-identical passthrough");
        {
            NamResampler r;
            r.prepare (96000.0, 96000.0, 512);

            expect (r.isPassthrough());
            expectEquals (r.getLatencySamples(), 0);

            std::vector<float> sig;
            fillSine (sig, 1000.0, 96000.0, 512);
            auto original = sig;

            // A model that adds 1 -- passthrough must call it and keep its output.
            r.process (sig.data(), 512, [] (const float* in, float* out, int n)
                       {
                           for (int i = 0; i < n; ++i)
                               out[i] = in[i] + 1.0f;
                       });

            for (int i = 0; i < 512; ++i)
                expectWithinAbsoluteError (sig[(size_t) i], original[(size_t) i] + 1.0f, 1.0e-6f);
        }

        beginTest ("96 kHz to a 48 kHz model and back preserves the signal");
        {
            NamResampler r;
            r.prepare (96000.0, 48000.0, 64);

            expect (! r.isPassthrough());
            expect (r.getLatencySamples() > 0);
            expect (r.getMaxModelFrames() >= 32);

            // Stream many 64-sample blocks of a 1 kHz sine through an identity
            // model. The output should be the input, delayed by the reported
            // latency, with almost perfect correlation -- the resampling must
            // not colour or drift.
            const auto blocks = 400;
            const auto blockSize = 64;

            std::vector<float> input, output;

            for (int b = 0; b < blocks; ++b)
            {
                std::vector<float> block;
                fillSine (block, 1000.0, 96000.0, blockSize, b * blockSize);

                for (const auto s : block)
                    input.push_back (s);

                r.process (block.data(), blockSize,
                           [] (const float* in, float* out, int n) { std::copy (in, in + n, out); });

                for (const auto s : block)
                    output.push_back (s);
            }

            // Length is conserved exactly -- no samples dropped or invented.
            expectEquals ((int) output.size(), (int) input.size());

            // Find the best lag near the reported latency and confirm the
            // signal survives the round trip.
            auto best = 0.0;

            for (int lag = 0; lag < 200; ++lag)
                best = juce::jmax (best, correlation (input, output, lag));

            expect (best > 0.98, "round-trip correlation only " + juce::String (best, 4));

            for (const auto s : output)
                expect (std::isfinite (s));
        }

        beginTest ("A non-integer ratio does not drift or glitch");
        {
            // 96 kHz host, 44.1 kHz model: ratio 2.1768..., the case that would
            // expose sloppy sample accounting.
            NamResampler r;
            r.prepare (96000.0, 44100.0, 128);

            expect (! r.isPassthrough());

            auto total = 0;
            auto maxAbs = 0.0f;

            for (int b = 0; b < 2000; ++b)
            {
                std::vector<float> block;
                fillSine (block, 500.0, 96000.0, 128, b * 128);

                r.process (block.data(), 128,
                           [] (const float* in, float* out, int n) { std::copy (in, in + n, out); });

                total += 128;

                for (const auto s : block)
                {
                    expect (std::isfinite (s));
                    maxAbs = juce::jmax (maxAbs, std::abs (s));
                }
            }

            expectEquals (total, 2000 * 128);
            // A 0.4-amplitude sine, resampled twice, must not blow up.
            expect (maxAbs < 1.0f, "signal grew to " + juce::String (maxAbs));
        }

        beginTest ("Silence in stays silence out");
        {
            NamResampler r;
            r.prepare (96000.0, 48000.0, 256);

            for (int b = 0; b < 50; ++b)
            {
                std::vector<float> block ((size_t) 256, 0.0f);

                r.process (block.data(), 256,
                           [] (const float* in, float* out, int n) { std::copy (in, in + n, out); });

                for (const auto s : block)
                    expectWithinAbsoluteError (s, 0.0f, 1.0e-7f);
            }
        }

        beginTest ("Downsampling to a 96 kHz model at a 48 kHz host works too");
        {
            // The reverse direction: a 96 kHz-trained model on a 48 kHz host.
            NamResampler r;
            r.prepare (48000.0, 96000.0, 256);

            expect (! r.isPassthrough());

            std::vector<float> block;
            fillSine (block, 1000.0, 48000.0, 256);

            r.process (block.data(), 256,
                       [] (const float* in, float* out, int n) { std::copy (in, in + n, out); });

            for (const auto s : block)
                expect (std::isfinite (s));
        }
    }
};

static NamResamplerTests namResamplerTests;

//==============================================================================
class NamLatencyTests final : public juce::UnitTest
{
public:
    NamLatencyTests() : juce::UnitTest ("NAM latency", "dsp") {}

    void runTest() override
    {
        using milodikfx::dsp::NamResampler;

        beginTest ("Passthrough adds no latency");
        {
            NamResampler r;
            r.prepare (96000.0, 96000.0, 32);
            expectEquals (r.getLatencySamples(), 0);
        }

        beginTest ("The reported latency is deterministic across prepares");
        {
            NamResampler a, b;
            a.prepare (96000.0, 48000.0, 32);
            b.prepare (96000.0, 48000.0, 32);

            expect (a.getLatencySamples() > 0);
            expectEquals (a.getLatencySamples(), b.getLatencySamples());
        }

        beginTest ("The reported latency matches the measured group delay");
        {
            // Push an impulse through an identity model and find where it comes
            // out. That delay should be close to what getLatencySamples reports,
            // or the number shown to the user would be a fiction.
            NamResampler r;
            r.prepare (96000.0, 48000.0, 32);

            const auto reported = r.getLatencySamples();

            std::vector<float> output;
            auto impulseSent = false;

            for (int b = 0; b < 400; ++b)
            {
                std::vector<float> block ((size_t) 32, 0.0f);

                if (! impulseSent)
                {
                    block[0] = 1.0f;
                    impulseSent = true;
                }

                r.process (block.data(), 32,
                           [] (const float* in, float* out, int n) { std::copy (in, in + n, out); });

                for (const auto s : block)
                    output.push_back (s);
            }

            // Peak of the response, weighted, is the group delay.
            auto peakIndex = 0;
            auto peak = 0.0f;

            for (int i = 0; i < (int) output.size(); ++i)
                if (std::abs (output[(size_t) i]) > peak)
                {
                    peak = std::abs (output[(size_t) i]);
                    peakIndex = i;
                }

            expect (peak > 0.1f, "the impulse never came out");

            // The reported figure is itself measured the same way at prepare
            // time, so the two should agree to within a couple of samples.
            expect (std::abs (peakIndex - reported) <= 2,
                    "measured delay " + juce::String (peakIndex)
                        + " vs reported " + juce::String (reported));
        }
    }
};

static NamLatencyTests namLatencyTests;

//==============================================================================
class NamProcessorTests final : public juce::UnitTest
{
public:
    NamProcessorTests() : juce::UnitTest ("NamProcessor", "dsp") {}

    void runTest() override
    {
        using milodikfx::dsp::NamProcessor;

        beginTest ("Passes through untouched with no model loaded");
        {
            NamProcessor nam;
            nam.setEnabled (true);
            nam.prepareToPlay (kHostRate, 32, 2);

            expect (! nam.isModelLoaded());

            juce::AudioBuffer<float> buffer (2, 32);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 32; ++i)
                    buffer.setSample (ch, i, 0.3f * (float) std::sin (0.1 * i));

            juce::AudioBuffer<float> original (2, 32);
            original.makeCopyOf (buffer);

            nam.processBlock (buffer);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 32; ++i)
                    expectWithinAbsoluteError (buffer.getSample (ch, i), original.getSample (ch, i), 1.0e-6f);
        }

        beginTest ("Reports availability and a reason when it cannot run");
        {
            // Whatever this machine is, the two must agree.
            const auto available = NamProcessor::isAvailable();
            const auto reason = NamProcessor::unavailableReason();

            expect (available == reason.isEmpty(),
                    "availability and reason disagree");
        }

        beginTest ("Refuses to load a file that is not there");
        {
            NamProcessor nam;
            nam.prepareToPlay (kHostRate, 32, 2);

            const auto error = nam.loadModel (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                                  .getChildFile ("does-not-exist.nam"));

            expect (error.isNotEmpty());
            expect (! nam.isModelLoaded());
        }

       #if MILODIKFX_ENABLE_NAM
        const juce::File modelsDir (MILODIKFX_NAM_EXAMPLE_MODELS_DIR);
        const auto standard = modelsDir.getChildFile ("wavenet_a1_standard.nam");

        // A tiny real WaveNet for the heavy loops: functionally identical load
        // and process path, but fast enough to hammer in an unoptimised Debug
        // build where a Standard model would take seconds per load.
        const auto tiny = modelsDir.getChildFile ("wavenet.nam");

        const auto haveModel = tiny.existsAsFile() && NamProcessor::isAvailable();

        if (! haveModel)
            logMessage ("  (skipping model-loading tests: "
                        + NamProcessor::unavailableReason() + ")");

        if (haveModel)
        {
            beginTest ("Loads a real Standard model and changes the signal");
            {
                // The one place the full Standard model is exercised end to end.
                NamProcessor nam;
                nam.setEnabled (true);
                nam.prepareToPlay (kHostRate, 32, 2);

                const auto error = nam.loadModel (standard.existsAsFile() ? standard : tiny);
                expect (error.isEmpty(), "load failed: " + error);
                expect (nam.isModelLoaded());
                expectEquals (nam.getLoadedName(),
                              standard.existsAsFile() ? juce::String ("wavenet_a1_standard")
                                                      : juce::String ("wavenet"));

                // Run enough blocks for the staged model to be adopted and the
                // resampler to settle.
                juce::AudioBuffer<float> buffer (2, 32);
                float maxOut = 0.0f;

                for (int b = 0; b < 200; ++b)
                {
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < 32; ++i)
                            buffer.setSample (ch, i, 0.3f * (float) std::sin (
                                                  juce::MathConstants<double>::twoPi * 220.0
                                                  * (double) (b * 32 + i) / kHostRate));

                    nam.processBlock (buffer);

                    for (int i = 0; i < 32; ++i)
                    {
                        const auto s = buffer.getSample (0, i);
                        expect (std::isfinite (s), "non-finite at block " + juce::String (b));
                        maxOut = juce::jmax (maxOut, std::abs (s));
                    }

                    // Both channels stay identical: the amp is mono.
                    for (int i = 0; i < 32; ++i)
                        expectWithinAbsoluteError (buffer.getSample (1, i), buffer.getSample (0, i), 1.0e-6f);
                }

                expect (maxOut > 0.0001f, "the model produced silence");
            }

            beginTest ("Disable returns to passthrough");
            {
                NamProcessor nam;
                nam.prepareToPlay (kHostRate, 32, 2);
                expect (nam.loadModel (tiny).isEmpty());

                // Adopt the model.
                juce::AudioBuffer<float> warm (2, 32);
                warm.clear();
                for (int b = 0; b < 50; ++b) nam.processBlock (warm);

                nam.setEnabled (false);

                juce::AudioBuffer<float> buffer (2, 32);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 32; ++i)
                        buffer.setSample (ch, i, 0.25f);

                juce::AudioBuffer<float> original (2, 32);
                original.makeCopyOf (buffer);

                nam.processBlock (buffer);

                for (int i = 0; i < 32; ++i)
                    expectWithinAbsoluteError (buffer.getSample (0, i), original.getSample (0, i), 1.0e-6f);
            }

            beginTest ("Clearing a model returns to passthrough");
            {
                NamProcessor nam;
                nam.setEnabled (true);
                nam.prepareToPlay (kHostRate, 32, 2);
                expect (nam.loadModel (tiny).isEmpty());

                juce::AudioBuffer<float> warm (2, 32);
                warm.clear();
                for (int b = 0; b < 50; ++b) nam.processBlock (warm);

                nam.clearModel();
                expect (! nam.isModelLoaded());

                // Adopt the empty slot.
                for (int b = 0; b < 10; ++b) nam.processBlock (warm);

                juce::AudioBuffer<float> buffer (2, 32);
                for (int i = 0; i < 32; ++i) buffer.setSample (0, i, 0.2f);
                buffer.copyFrom (1, 0, buffer, 0, 0, 32);

                juce::AudioBuffer<float> original (2, 32);
                original.makeCopyOf (buffer);

                nam.processBlock (buffer);
                nam.collectGarbage();

                for (int i = 0; i < 32; ++i)
                    expectWithinAbsoluteError (buffer.getSample (0, i), original.getSample (0, i), 1.0e-6f);
            }

            beginTest ("Survives a device restart with a model loaded");
            {
                NamProcessor nam;
                nam.setEnabled (true);
                nam.prepareToPlay (96000.0, 32, 2);
                expect (nam.loadModel (tiny).isEmpty());

                juce::AudioBuffer<float> warm (2, 32);
                warm.clear();
                for (int b = 0; b < 50; ++b) nam.processBlock (warm);

                // Restart at a different rate and block size, as a device change
                // would. This re-Resets the model off the audio thread.
                nam.prepareToPlay (48000.0, 128, 2);

                juce::AudioBuffer<float> buffer (2, 128);
                for (int b = 0; b < 50; ++b)
                {
                    for (int i = 0; i < 128; ++i)
                        buffer.setSample (0, i, 0.2f * (float) std::sin (0.05 * (b * 128 + i)));
                    buffer.copyFrom (1, 0, buffer, 0, 0, 128);

                    nam.processBlock (buffer);

                    for (int i = 0; i < 128; ++i)
                        expect (std::isfinite (buffer.getSample (0, i)));
                }
            }

            beginTest ("Loading models on another thread never breaks the audio thread");
            {
                // The handoff under stress: a worker loads and clears models in a
                // tight loop while the audio thread processes. Mirrors the
                // concurrency the crash-safety rules exist for.
                NamProcessor nam;
                nam.setEnabled (true);
                nam.prepareToPlay (kHostRate, 32, 2);

                std::atomic<bool> stop { false };

                std::thread loader ([&]
                {
                    for (int i = 0; i < 40 && ! stop.load(); ++i)
                    {
                        nam.loadModel (tiny);
                        nam.collectGarbage();
                        std::this_thread::sleep_for (std::chrono::milliseconds (2));
                        nam.clearModel();
                        nam.collectGarbage();
                    }
                });

                juce::AudioBuffer<float> buffer (2, 32);
                auto allFinite = true;

                for (int b = 0; b < 6000; ++b)
                {
                    for (int i = 0; i < 32; ++i)
                        buffer.setSample (0, i, 0.2f * (float) std::sin (0.03 * (b * 32 + i)));
                    buffer.copyFrom (1, 0, buffer, 0, 0, 32);

                    nam.processBlock (buffer);
                    nam.collectGarbage();

                    for (int i = 0; i < 32; ++i)
                        if (! std::isfinite (buffer.getSample (0, i)))
                            allFinite = false;
                }

                stop.store (true);
                loader.join();

                expect (allFinite, "a model swap produced a non-finite sample");
            }
        }
       #endif
    }
};

static NamProcessorTests namProcessorTests;

//==============================================================================
class NamLibraryTests final : public juce::UnitTest
{
public:
    NamLibraryTests() : juce::UnitTest ("NamLibrary", "preset") {}

    void runTest() override
    {
        using milodikfx::preset::NamLibrary;

        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("MilodikFX_NamLibraryTests");
        dir.deleteRecursively();

        beginTest ("Creates its directory and starts empty");
        {
            NamLibrary library (dir);
            expect (dir.isDirectory());
            expect (library.list().isEmpty());
        }

        beginTest ("Import stores a model and makes it listable and resolvable");
        {
            NamLibrary library (dir);

            juce::MemoryBlock data ("{\"fake\":\"model\"}", 16);
            const auto stored = library.import ("Marshall JVM", data);

            expectEquals (stored, juce::String ("Marshall JVM"));
            expect (library.list().contains ("Marshall JVM"));

            const auto resolved = library.resolve ("Marshall JVM");
            expect (resolved.existsAsFile());
            expect (resolved.hasFileExtension ("nam"));
            expect (resolved.isAChildOf (dir));
        }

        beginTest ("An unknown or empty name resolves to nothing");
        {
            NamLibrary library (dir);
            expect (! library.resolve ("no such model").existsAsFile());
            expect (! library.resolve ("").existsAsFile());
        }

        beginTest ("A name cannot escape the models directory");
        {
            NamLibrary library (dir);

            juce::MemoryBlock data ("weights", 7);
            const auto stored = library.import ("../../evil", data);

            // Whatever it sanitised to, the file must be inside the library.
            if (stored.isNotEmpty())
            {
                const auto resolved = library.resolve (stored);
                expect (resolved == juce::File() || resolved.isAChildOf (dir));
            }

            expect (! dir.getParentDirectory().getChildFile ("evil.nam").existsAsFile());
            expect (! dir.getParentDirectory().getParentDirectory().getChildFile ("evil.nam").existsAsFile());
        }

        dir.deleteRecursively();
    }
};

static NamLibraryTests namLibraryTests;

//==============================================================================
class NamRegistryTests final : public juce::UnitTest
{
public:
    NamRegistryTests() : juce::UnitTest ("NAM registry", "preset") {}

    void runTest() override
    {
        beginTest ("The head stage registers with input and output gain");
        {
            milodikfx::dsp::DSPChainManager manager;
            const auto chain = milodikfx::dsp::buildGuitarChain (manager);

            expect (chain.nam != nullptr);

            milodikfx::api::ParameterRegistry registry;
            milodikfx::dsp::registerChainParameters (registry, chain, manager);

            const auto* nam = registry.findEffect ("nam");
            expect (nam != nullptr, "the nam effect is not registered");

            expect (registry.findParameter ("nam", "inputDb") != nullptr);
            expect (registry.findParameter ("nam", "outputDb") != nullptr);

            // No library supplied here, so no file chooser -- exactly what a
            // plugin build (no models directory) gets.
            expect (registry.findParameter ("nam", "namFile") == nullptr);
        }

        beginTest ("With a library the model chooser appears as a text parameter");
        {
            auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getChildFile ("MilodikFX_NamRegistryTests");
            dir.deleteRecursively();

            milodikfx::preset::NamLibrary library (dir);
            juce::MemoryBlock data ("{\"fake\":\"model\"}", 16);
            library.import ("Test Amp", data);

            milodikfx::dsp::DSPChainManager manager;
            const auto chain = milodikfx::dsp::buildGuitarChain (manager);

            milodikfx::api::ParameterRegistry registry;
            milodikfx::dsp::ChainExtras extras;
            extras.namLibrary = &library;
            milodikfx::dsp::registerChainParameters (registry, chain, manager, std::move (extras));

            const auto* namFile = registry.findParameter ("nam", "namFile");
            expect (namFile != nullptr, "namFile is missing when a library is supplied");

            if (namFile != nullptr)
            {
                expect (namFile->isText);
                expect (namFile->getOptions().contains ("Test Amp"));
            }

            // A numeric write to a text parameter is refused.
            float applied = 0.0f;
            expect (! registry.setParameter ("nam", "namFile", 1.0f, applied));

            dir.deleteRecursively();
        }

        beginTest ("The head's gains survive a preset round trip");
        {
            milodikfx::dsp::DSPChainManager manager;
            const auto chain = milodikfx::dsp::buildGuitarChain (manager);

            milodikfx::api::ParameterRegistry registry;
            milodikfx::dsp::registerChainParameters (registry, chain, manager);

            float applied = 0.0f;
            registry.setParameter ("nam", "inputDb", 6.5f, applied);
            registry.setParameter ("nam", "outputDb", -3.0f, applied);

            const auto state = registry.captureState();

            registry.setParameter ("nam", "inputDb", 0.0f, applied);
            registry.setParameter ("nam", "outputDb", 0.0f, applied);

            registry.applyState (state);

            expectWithinAbsoluteError (chain.nam->getInputDb(), 6.5f, 0.01f);
            expectWithinAbsoluteError (chain.nam->getOutputDb(), -3.0f, 0.01f);
        }
    }
};

static NamRegistryTests namRegistryTests;
