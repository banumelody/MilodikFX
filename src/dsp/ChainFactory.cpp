#include "dsp/ChainFactory.h"

#include <string>
#include <type_traits>

#include <cmath>

namespace milodikfx::dsp
{
namespace
{
using milodikfx::api::EffectDescriptor;
using milodikfx::api::ParameterDescriptor;

ParameterDescriptor makeParam (std::string id,
                               std::string label,
                               std::string unit,
                               float minValue,
                               float maxValue,
                               float step,
                               float defaultValue,
                               std::function<float()> get,
                               std::function<void (float)> set)
{
    ParameterDescriptor p;
    p.id = std::move (id);
    p.label = std::move (label);
    p.unit = std::move (unit);
    p.minValue = minValue;
    p.maxValue = maxValue;
    p.step = step;
    p.defaultValue = defaultValue;
    p.get = std::move (get);
    p.set = std::move (set);
    return p;
}

ParameterDescriptor makeToggle (std::string id,
                                std::string label,
                                bool defaultValue,
                                std::function<bool()> get,
                                std::function<void (bool)> set)
{
    ParameterDescriptor p;
    p.id = std::move (id);
    p.label = std::move (label);
    p.minValue = 0.0f;
    p.maxValue = 1.0f;
    p.step = 1.0f;
    p.defaultValue = defaultValue ? 1.0f : 0.0f;
    p.isBoolean = true;
    p.get = [get] { return get() ? 1.0f : 0.0f; };
    p.set = [set] (float v) { set (v >= 0.5f); };
    return p;
}

template <typename ProcessorType>
ProcessorType* add (DSPChainManager& chain)
{
    return dynamic_cast<ProcessorType*> (chain.addProcessor (std::make_unique<ProcessorType>()));
}

/**
 * Builds the "which impulse response" control for a processor that owns an
 * IrEngine. Selecting a name loads it immediately; an unknown or unreadable
 * name clears the engine, which makes the processor fall back to its own
 * algorithm rather than going silent.
 */
template <typename ProcessorType>
ParameterDescriptor makeIrFileParam (ProcessorType* processor,
                                     milodikfx::preset::IrLibrary& library,
                                     milodikfx::preset::IrLibrary::Category category)
{
    ParameterDescriptor p;
    p.id = "irFile";
    p.label = "Impulse Response";
    p.isText = true;

    p.getText = [processor] { return processor->getIrEngine().getLoadedName(); };

    p.setText = [processor, &library, category] (const juce::String& name)
    {
        auto& engine = processor->getIrEngine();

        if (name.isEmpty())
        {
            engine.clear();
            return;
        }

        const auto file = library.resolve (category, name);

        if (! engine.loadFromFile (file))
            engine.clear();
    };

    p.getOptions = [&library, category] { return library.list (category); };

    return p;
}

/**
 * The "which .nam model" control. Selecting a name loads it on the calling REST
 * thread (never the audio thread); an unknown or unreadable name clears the
 * head, which falls back to passing the signal straight through.
 */
ParameterDescriptor makeNamFileParam (NamProcessor* processor,
                                      milodikfx::preset::NamLibrary& library)
{
    ParameterDescriptor p;
    p.id = "namFile";
    p.label = "Amp Model";
    p.isText = true;

    p.getText = [processor] { return processor->getLoadedName(); };

    p.setText = [processor, &library] (const juce::String& name)
    {
        if (name.isEmpty())
        {
            processor->clearModel();
            return;
        }

        const auto file = library.resolve (name);

        // Best-effort like the IR loader: a bad file leaves the head cleared
        // rather than throwing across the REST boundary.
        if (processor->loadModel (file).isNotEmpty())
            processor->clearModel();
    };

    p.getOptions = [&library] { return library.list(); };

    return p;
}
} // namespace

int ChainInventory::maxInstances() const noexcept
{
    // jmax only takes three at a time, and a fold reads better than nesting.
    const int counts[] = { 1, noiseGate, cleanBoost, compressor, overdrive,
                           eq, toneStack, cabinet, delay, reverb };

    int most = 1;

    for (const auto count : counts)
        most = juce::jmax (most, count);

    return most;
}

GuitarChain buildGuitarChain (DSPChainManager& chain, ChainOptions options, ChainInventory inventory)
{
    GuitarChain result;
    result.extras.resize ((size_t) juce::jmax (0, inventory.maxInstances() - 1));

    // Instances of one type are built next to each other, so the chain's
    // starting order reads Overdrive, Overdrive 2, Overdrive 3 rather than
    // scattering them -- a default anyone can then drag apart. `member` names
    // the same field on every layer, so instance 1 and its siblings are filled
    // by one call.
    const auto addInstances = [&chain, &result] (auto GuitarChain::* member, int count)
    {
        using Slot = std::remove_reference_t<decltype (result.*member)>;
        using Processor = std::remove_pointer_t<Slot>;

        result.*member = add<Processor> (chain);

        for (int i = 1; i < count; ++i)
            result.extras[(size_t) (i - 1)].*member = add<Processor> (chain);
    };

    // Signal order matters: trim the guitar to the chain before the gate sees
    // it, so the gate threshold stays correct relative to the signal; gate the
    // raw pickup before anything boosts its noise; compress before the clipper
    // so the drive sees a steady level; and put the cabinet after all the
    // distortion it is meant to be filtering.
    result.inputTrim = add<InputTrimProcessor> (chain);
    addInstances (&GuitarChain::noiseGate, inventory.noiseGate);
    addInstances (&GuitarChain::cleanBoost, inventory.cleanBoost);
    addInstances (&GuitarChain::compressor, inventory.compressor);

    // The parallel section's opening bracket. Sits here by default -- after the
    // dynamics, before the drive -- which is where splitting is most useful:
    // clean low end down one path, drive down the other. Both brackets are
    // draggable, so this is a starting point rather than a rule.
    result.split = add<SplitProcessor> (chain);
    addInstances (&GuitarChain::overdrive, inventory.overdrive);
    addInstances (&GuitarChain::eq, inventory.eq);
    addInstances (&GuitarChain::toneStack, inventory.toneStack);
    // The amp head sits between the tone shaping and the cabinet, exactly where
    // a real head sits between the pedals and the speaker.
    result.nam = add<NamProcessor> (chain);
    addInstances (&GuitarChain::cabinet, inventory.cabinet);
    addInstances (&GuitarChain::delay, inventory.delay);
    addInstances (&GuitarChain::reverb, inventory.reverb);
    // The closing bracket, immediately before the master stage.
    result.mixer = add<MixerProcessor> (chain);

    result.masterOut = add<MasterOutProcessor> (chain);

    // The trim and the master stage are the two that may never be moved once the
    // order becomes editable. The trim, because the input meter reports what the
    // chain receives as `inputDb + trimDb` rather than measuring a second time,
    // and that arithmetic is only true while the trim is first. The master,
    // because it carries the safety limiter and the final clamp.
    chain.setFixedStages (1, 1);
    chain.setParallelStages (result.split, result.mixer);

    // Not a stage of the chain: mixed in afterwards so bypass cannot silence it.
    if (options.withMetronome)
        result.metronome = dynamic_cast<MetronomeProcessor*> (
            chain.addPostProcessor (std::make_unique<MetronomeProcessor>()));

    // The looper is post-master too, so the loop keeps playing through a bypass
    // and is not distorted by the amp and cabinet it would run through in-chain.
    if (options.withLooper)
        result.looper = dynamic_cast<LooperProcessor*> (
            chain.addPostProcessor (std::make_unique<LooperProcessor>()));

    return result;
}

namespace
{
/**
 * Registers one instance of every stage a layer holds.
 *
 * Called once per instance, and that is the whole trick behind duplicate
 * blocks: the descriptions live in exactly one place, so a second overdrive is
 * provably the same overdrive rather than a copy that can drift. Instance 1
 * carries today's bare id (`overdrive`) and later ones get a numeric suffix
 * (`overdrive2`), which is what lets every preset, settings file, MIDI mapping
 * and DAW automation lane written before v0.31 keep working untouched.
 *
 * A layer that has no instance of a type leaves its pointer null and the block
 * is skipped, so the non-duplicable stages -- the amp head, the split brackets,
 * the master limiter -- appear only in the first layer without needing a guard.
 */
void registerLayer (milodikfx::api::ParameterRegistry& registry,
                    const GuitarChain& chain,
                    DSPChainManager& manager,
                    ChainExtras& extras,
                    const std::string& suffix,
                    bool first)
{
    juce::ignoreUnused (manager);

    /** Instance 2 is labelled "Overdrive 2"; instance 1 stays "Overdrive". */
    const auto instanced = [&suffix] (const char* label)
    {
        return suffix.empty() ? std::string (label) : std::string (label) + " " + suffix;
    };

    auto getInputMode = std::move (extras.getInputMode);
    auto setInputMode = std::move (extras.setInputMode);

    // Global controls that belong to the chain as a whole rather than to any one
    // effect. Always in the path, so never toggleable as a unit.
    if (first)
    {
        EffectDescriptor e;
        e.id = "global";
        e.label = "Global";
        e.description = "Kontrol yang berlaku untuk seluruh rantai";
        e.isEnabled = [] { return true; };
        e.setEnabled = nullptr;
        e.parameters.push_back (makeToggle ("bypass", "Bypass", false,
                                            [&manager] { return manager.isBypassed(); },
                                            [&manager] (bool v) { manager.setBypassed (v); }));

        // One tempo for the whole chain. The metronome stores it and the delay is
        // handed a copy, so a synced repeat always lands on the click rather
        // than drifting against a second, separately-edited BPM.
        //
        // A chain built without a metronome (a plugin: the host has its own
        // click) still needs the tempo, because a synced delay and a synced LFO
        // read it. The delay then holds it instead -- one owner either way, so
        // there is still never a second BPM to disagree with.
        // **Every** delay, not just the first. One BPM for the whole app is the
        // rule -- two independently-edited tempi would let a synced repeat drift
        // against the click -- and with more than one delay instance, reaching
        // only `chain.delay` would leave the others running on a stale tempo.
        std::vector<DelayProcessor*> delays;

        if (chain.delay != nullptr)
            delays.push_back (chain.delay);

        for (const auto& layer : chain.extras)
            if (layer.delay != nullptr)
                delays.push_back (layer.delay);

        if (auto* metronome = chain.metronome)
        {
            e.parameters.push_back (makeParam ("bpm", "Tempo", "BPM",
                                               MetronomeProcessor::kMinBpm,
                                               MetronomeProcessor::kMaxBpm,
                                               1.0f, 120.0f,
                                               [metronome] { return metronome->getBpm(); },
                                               [metronome, delays] (float v)
                                               {
                                                   metronome->setBpm (v);

                                                   for (auto* delay : delays)
                                                       delay->setBpm (v);
                                               }));
        }
        else if (! delays.empty())
        {
            // No metronome (a plugin): the first delay owns the tempo, and the
            // rest follow it so there is still exactly one.
            auto* owner = delays.front();

            e.parameters.push_back (makeParam ("bpm", "Tempo", "BPM",
                                               MetronomeProcessor::kMinBpm,
                                               MetronomeProcessor::kMaxBpm,
                                               1.0f, 120.0f,
                                               [owner] { return owner->getBpm(); },
                                               [delays] (float v)
                                               {
                                                   for (auto* delay : delays)
                                                       delay->setBpm (v);
                                               }));
        }

        registry.addEffect (std::move (e));
    }

    // Effect and parameter ids double as settings keys (dsp.<effect>.<param>)
    // and as REST path segments, so they stay stable even when labels change.
    //
    // The card exists whenever the trim processor does, so a plugin gets the
    // trim too. Channel routing is host-specific -- the app maps device
    // channels itself, a plugin gets whatever the host sends -- so Mode is
    // added only when the host supplies the accessors.
    if (chain.inputTrim != nullptr || (getInputMode && setInputMode))
    {
        EffectDescriptor e;
        e.id = "input";
        e.label = "Input";
        e.description = "Samakan level gitar ini dengan rantai - setel sekali per gitar";
        e.isEnabled = [] { return true; };
        e.setEnabled = nullptr; // always in the path; nothing to bypass

        if (auto* p = chain.inputTrim)
            e.parameters.push_back (makeParam ("gainDb", "Gain", "dB",
                                               InputTrimProcessor::kMinDb,
                                               InputTrimProcessor::kMaxDb,
                                               0.1f, 0.0f,
                                               [p] { return p->getGainDb(); },
                                               [p] (float v) { p->setGainDb (v); }));

        if (getInputMode && setInputMode)
            e.parameters.push_back (makeParam ("mode", "Mode", "", 0.0f, 3.0f, 1.0f, 0.0f,
                                               std::move (getInputMode), std::move (setInputMode)));

        registry.addEffect (std::move (e));
    }

    if (auto* p = chain.noiseGate)
    {
        EffectDescriptor e;
        e.id = "noiseGate" + suffix;
        e.label = instanced ("Noise Gate");
        e.description = "Meredam dengung pickup di sela nada";
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };
        e.parameters.push_back (makeParam ("thresholdDb", "Threshold", "dB", -90.0f, 0.0f, 0.5f, -55.0f,
                                           [p] { return p->getThresholdDb(); },
                                           [p] (float v) { p->setThresholdDb (v); }));
        e.parameters.push_back (makeParam ("attackMs", "Attack", "ms", 0.1f, 50.0f, 0.1f, 2.0f,
                                           [p] { return p->getAttackMs(); },
                                           [p] (float v) { p->setAttackMs (v); }));
        e.parameters.push_back (makeParam ("holdMs", "Hold", "ms", 0.0f, 500.0f, 1.0f, 60.0f,
                                           [p] { return p->getHoldMs(); },
                                           [p] (float v) { p->setHoldMs (v); }));
        e.parameters.push_back (makeParam ("releaseMs", "Release", "ms", 5.0f, 1000.0f, 1.0f, 150.0f,
                                           [p] { return p->getReleaseMs(); },
                                           [p] (float v) { p->setReleaseMs (v); }));
        registry.addEffect (std::move (e));
    }

    if (auto* p = chain.cleanBoost)
    {
        EffectDescriptor e;
        e.id = "cleanBoost" + suffix;
        e.label = instanced ("Clean Boost");
        // Distinct from Input Gain on purpose: that one matches the guitar to
        // the chain and is set once, this one is pushed in for a solo.
        e.description = "Dorong front-end untuk solo - hanya menambah, setelah noise gate";
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };
        e.parameters.push_back (makeParam ("gainDb", "Gain", "dB", 0.0f, 24.0f, 0.1f, 0.0f,
                                           [p] { return p->getGainDb(); },
                                           [p] (float v) { p->setGainDb (v); }));
        registry.addEffect (std::move (e));
    }

    if (auto* p = chain.compressor)
    {
        EffectDescriptor e;
        e.id = "compressor" + suffix;
        e.label = instanced ("Compressor");
        e.description = "Meratakan dinamika petikan";
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };
        // "Drive" rather than "Input": this sets how hard the signal hits this
        // compressor's threshold, which is a different job from the chain-wide
        // Input Gain, and two controls both labelled "Input" would not say so.
        e.parameters.push_back (makeParam ("inputGainDb", "Drive", "dB", -24.0f, 24.0f, 0.1f, 0.0f,
                                           [p] { return p->getInputGainDb(); },
                                           [p] (float v) { p->setInputGainDb (v); }));
        e.parameters.push_back (makeParam ("thresholdDb", "Threshold", "dB", -60.0f, 0.0f, 0.5f, -24.0f,
                                           [p] { return p->getThresholdDb(); },
                                           [p] (float v) { p->setThresholdDb (v); }));
        e.parameters.push_back (makeParam ("ratio", "Ratio", ":1", 1.0f, 20.0f, 0.1f, 4.0f,
                                           [p] { return p->getRatio(); },
                                           [p] (float v) { p->setRatio (v); }));
        e.parameters.push_back (makeParam ("attackMs", "Attack", "ms", 0.1f, 200.0f, 0.1f, 10.0f,
                                           [p] { return p->getAttackMs(); },
                                           [p] (float v) { p->setAttackMs (v); }));
        e.parameters.push_back (makeParam ("releaseMs", "Release", "ms", 5.0f, 2000.0f, 1.0f, 100.0f,
                                           [p] { return p->getReleaseMs(); },
                                           [p] (float v) { p->setReleaseMs (v); }));
        e.parameters.push_back (makeParam ("mixPct", "Mix", "%", 0.0f, 100.0f, 1.0f, 100.0f,
                                           [p] { return p->getMixPercent(); },
                                           [p] (float v) { p->setMixPercent (v); }));
        e.parameters.push_back (makeToggle ("autoMakeup", "Auto Makeup", true,
                                            [p] { return p->getAutoMakeupGain(); },
                                            [p] (bool v) { p->setAutoMakeupGain (v); }));
        registry.addEffect (std::move (e));
    }

    if (auto* p = chain.split)
    {
        EffectDescriptor e;
        e.id = "split";
        e.label = "Split";
        e.description = "Belah sinyal jadi dua jalur - A dan B";
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };

        // 0 = the same signal down both paths, 1 = low to A and high to B,
        // 2 = left channel to A and right to B. The crossover is the bass move
        // (clean lows, driven highs); L/R is for two sources rather than one,
        // such as a guitar with a magnetic and a piezo pickup.
        e.parameters.push_back (makeParam ("mode", "Mode", "", 0.0f, 2.0f, 1.0f, 0.0f,
                                           [p] { return (float) (int) p->getMode(); },
                                           [p] (float v)
                                           {
                                               const auto index = juce::jlimit (0, 2, (int) std::lround (v));
                                               p->setMode ((SplitProcessor::Mode) index);
                                           }));

        e.parameters.push_back (makeParam ("freqHz", "Frekuensi", "Hz",
                                           SplitProcessor::kMinFrequencyHz,
                                           SplitProcessor::kMaxFrequencyHz,
                                           1.0f, 250.0f,
                                           [p] { return p->getFrequencyHz(); },
                                           [p] (float v) { p->setFrequencyHz (v); }));

        registry.addEffect (std::move (e));
    }

    if (auto* p = chain.mixer)
    {
        EffectDescriptor e;
        e.id = "mixer";
        e.label = "Mixer";
        e.description = "Gabungkan jalur A dan B - level dan pan tiap jalur";
        e.isEnabled = [] { return true; };
        e.setEnabled = nullptr; // always in the path; the split is what switches

        e.parameters.push_back (makeParam ("levelA", "Level A", "", 0.0f, 2.0f, 0.01f, 1.0f,
                                           [p] { return p->getLevelA(); },
                                           [p] (float v) { p->setLevelA (v); }));
        e.parameters.push_back (makeParam ("panA", "Pan A", "", -1.0f, 1.0f, 0.01f, 0.0f,
                                           [p] { return p->getPanA(); },
                                           [p] (float v) { p->setPanA (v); }));
        e.parameters.push_back (makeParam ("levelB", "Level B", "", 0.0f, 2.0f, 0.01f, 1.0f,
                                           [p] { return p->getLevelB(); },
                                           [p] (float v) { p->setLevelB (v); }));
        e.parameters.push_back (makeParam ("panB", "Pan B", "", -1.0f, 1.0f, 0.01f, 0.0f,
                                           [p] { return p->getPanB(); },
                                           [p] (float v) { p->setPanB (v); }));

        // Two pickups on one guitar can partially cancel when blended; this is
        // the fix, and it belongs on the mixer because that is where they meet.
        e.parameters.push_back (makeParam ("invertB", "Invert B", "", 0.0f, 1.0f, 1.0f, 0.0f,
                                           [p] { return p->getInvertB() ? 1.0f : 0.0f; },
                                           [p] (float v) { p->setInvertB (v >= 0.5f); }));

        registry.addEffect (std::move (e));
    }

    if (auto* p = chain.overdrive)
    {
        EffectDescriptor e;
        e.id = "overdrive" + suffix;
        e.label = instanced ("Overdrive");
        e.description = "Overdrive, distorsi, dan fuzz - pilih voicing pedalnya, kontrol menyesuaikan tipe";
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };

        // Every voicing's controls are registered here, once. Which of them a
        // given type actually uses is a presentation question, so the UI hides
        // the rest -- the registry stays a flat, stable set of ids that presets
        // and settings can rely on.
        e.parameters.push_back (makeParam ("type", "Tipe", "", 0.0f,
                                           (float) (drive::numTypes - 1), 1.0f, 0.0f,
                                           [p] { return (float) p->getType(); },
                                           [p] (float v) { p->setType ((int) std::lround (v)); }));
        e.parameters.push_back (makeParam ("drivePct", "Drive", "%", 0.0f, 100.0f, 0.5f, 0.0f,
                                           [p] { return p->getDrivePercent(); },
                                           [p] (float v) { p->setDrivePercent (v); }));
        e.parameters.push_back (makeParam ("levelPct", "Level", "%", 0.0f, 100.0f, 0.5f, 100.0f,
                                           [p] { return p->getLevelPercent(); },
                                           [p] (float v) { p->setLevelPercent (v); }));
        e.parameters.push_back (makeParam ("asymmetry", "Asymmetry", "", 0.0f, 1.0f, 0.01f, 0.0f,
                                           [p] { return p->getAsymmetry(); },
                                           [p] (float v) { p->setAsymmetry (v); }));
        // Enum 0..3 = Off / 2x / 4x / 8x. The old boolean stored 1 under this
        // same key, which now reads back as 2x -- exactly the previous behaviour.
        e.parameters.push_back (makeParam ("oversampling", "Oversampling", "x", 0.0f, 3.0f, 1.0f, 1.0f,
                                           [p] { return (float) p->getOversamplingIndex(); },
                                           [p] (float v) { p->setOversamplingIndex ((int) std::lround (v)); }));
        e.parameters.push_back (makeParam ("tonePct", "Tone", "%", 0.0f, 100.0f, 1.0f, 50.0f,
                                           [p] { return p->getTonePercent(); },
                                           [p] (float v) { p->setTonePercent (v); }));
        e.parameters.push_back (makeParam ("voicePct", "Voice", "%", 0.0f, 100.0f, 1.0f, 50.0f,
                                           [p] { return p->getVoicePercent(); },
                                           [p] (float v) { p->setVoicePercent (v); }));
        e.parameters.push_back (makeParam ("bassDb", "Bass", "dB", -12.0f, 12.0f, 0.1f, 0.0f,
                                           [p] { return p->getBassDb(); },
                                           [p] (float v) { p->setBassDb (v); }));
        e.parameters.push_back (makeParam ("midDb", "Mid", "dB", -12.0f, 12.0f, 0.1f, 0.0f,
                                           [p] { return p->getMidDb(); },
                                           [p] (float v) { p->setMidDb (v); }));
        e.parameters.push_back (makeParam ("trebleDb", "Treble", "dB", -12.0f, 12.0f, 0.1f, 0.0f,
                                           [p] { return p->getTrebleDb(); },
                                           [p] (float v) { p->setTrebleDb (v); }));
        e.parameters.push_back (makeToggle ("hpMode", "HP Mode", false,
                                            [p] { return p->isHighPeakMode(); },
                                            [p] (bool v) { p->setHighPeakMode (v); }));
        e.parameters.push_back (makeToggle ("bright", "Bright", false,
                                            [p] { return p->isBright(); },
                                            [p] (bool v) { p->setBright (v); }));
        registry.addEffect (std::move (e));
    }

    if (auto* p = chain.eq)
    {
        EffectDescriptor e;
        e.id = "eq" + suffix;
        e.label = instanced ("EQ");
        e.description = "Pembentuk nada SEBELUM distorsi - 120 Hz / 1 kHz / 7 kHz";
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };
        e.parameters.push_back (makeParam ("bassDb", "Bass", "dB", -12.0f, 12.0f, 0.1f, 0.0f,
                                           [p] { return p->getBassDb(); },
                                           [p] (float v) { p->setBassDb (v); }));
        e.parameters.push_back (makeParam ("midDb", "Mid", "dB", -12.0f, 12.0f, 0.1f, 0.0f,
                                           [p] { return p->getMidDb(); },
                                           [p] (float v) { p->setMidDb (v); }));
        e.parameters.push_back (makeParam ("trebleDb", "Treble", "dB", -12.0f, 12.0f, 0.1f, 0.0f,
                                           [p] { return p->getTrebleDb(); },
                                           [p] (float v) { p->setTrebleDb (v); }));
        registry.addEffect (std::move (e));
    }

    if (auto* p = chain.toneStack)
    {
        EffectDescriptor e;
        e.id = "toneStack" + suffix;
        e.label = instanced ("Contour");
        e.description = "Pembentuk nada SETELAH distorsi, sebelum cabinet - 50 Hz / 500 Hz / 5 kHz";
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };
        e.parameters.push_back (makeParam ("bassDb", "Low", "dB", -12.0f, 12.0f, 0.1f, 0.0f,
                                           [p] { return p->getBassDb(); },
                                           [p] (float v) { p->setBassDb (v); }));
        e.parameters.push_back (makeParam ("midDb", "Mid", "dB", -12.0f, 12.0f, 0.1f, 0.0f,
                                           [p] { return p->getMidDb(); },
                                           [p] (float v) { p->setMidDb (v); }));
        e.parameters.push_back (makeParam ("trebleDb", "High", "dB", -12.0f, 12.0f, 0.1f, 0.0f,
                                           [p] { return p->getTrebleDb(); },
                                           [p] (float v) { p->setTrebleDb (v); }));
        registry.addEffect (std::move (e));
    }

    if (auto* p = chain.nam)
    {
        EffectDescriptor e;
        e.id = "nam";
        e.label = "Amp (NAM)";
        e.description = "Kepala amp hasil capture Neural Amp Modeler - sebelum cabinet";
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };

        e.parameters.push_back (makeParam ("inputDb", "Input", "dB", -24.0f, 24.0f, 0.1f, 0.0f,
                                           [p] { return p->getInputDb(); },
                                           [p] (float v) { p->setInputDb (v); }));
        e.parameters.push_back (makeParam ("outputDb", "Output", "dB", -24.0f, 24.0f, 0.1f, 0.0f,
                                           [p] { return p->getOutputDb(); },
                                           [p] (float v) { p->setOutputDb (v); }));

        if (extras.namLibrary != nullptr)
            e.parameters.push_back (makeNamFileParam (p, *extras.namLibrary));

        registry.addEffect (std::move (e));
    }

    if (auto* p = chain.cabinet)
    {
        EffectDescriptor e;
        e.id = "cabinet" + suffix;
        e.label = instanced ("Cabinet");
        e.description = "Simulasi speaker - biarkan menyala untuk gitar DI";
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };
        e.parameters.push_back (makeParam ("presenceDb", "Presence", "dB", -12.0f, 12.0f, 0.1f, 0.0f,
                                           [p] { return p->getPresenceDb(); },
                                           [p] (float v) { p->setPresenceDb (v); }));
        e.parameters.push_back (makeParam ("toneHz", "Tone", "Hz", 2000.0f, 8000.0f, 50.0f, 5500.0f,
                                           [p] { return p->getToneHz(); },
                                           [p] (float v) { p->setToneHz (v); }));

        if (extras.irLibrary != nullptr)
        {
            e.parameters.push_back (makeToggle ("irEnabled", "Pakai IR", false,
                                                [p] { return p->isUsingImpulseResponse(); },
                                                [p] (bool v) { p->setUseImpulseResponse (v); }));
            e.parameters.push_back (makeIrFileParam (p, *extras.irLibrary,
                                                     milodikfx::preset::IrLibrary::Category::cabinet));

            // A second IR and a blend between them. Close mic plus room mic is
            // standard studio practice and the cheapest way past the slightly
            // static quality a single impulse has.
            auto second = makeIrFileParam (p, *extras.irLibrary,
                                           milodikfx::preset::IrLibrary::Category::cabinet);
            second.id = "irFileB";
            second.label = "Impulse Response B";
            second.getText = [p] { return p->getIrEngineB().getLoadedName(); };
            second.setText = [p, library = extras.irLibrary] (const juce::String& name)
            {
                auto& engine = p->getIrEngineB();

                if (name.isEmpty())
                {
                    engine.clear();
                    return;
                }

                const auto file = library->resolve (milodikfx::preset::IrLibrary::Category::cabinet, name);

                if (! engine.loadFromFile (file))
                    engine.clear();
            };

            e.parameters.push_back (std::move (second));

            e.parameters.push_back (makeParam ("irBlend", "A/B Blend", "", 0.0f, 1.0f, 0.01f, 0.0f,
                                               [p] { return p->getIrBlend(); },
                                               [p] (float v) { p->setIrBlend (v); }));

            // 0 = blend the two IRs together, 1 = A left / B right. Stereo mode
            // is how a real stereo rig is built -- one mono amp into two
            // cabinets panned apart -- and it costs nothing, because a blend
            // already runs both convolution engines every block.
            e.parameters.push_back (makeParam ("irMode", "Mode IR", "", 0.0f, 1.0f, 1.0f, 0.0f,
                                               [p] { return (float) (int) p->getIrMode(); },
                                               [p] (float v)
                                               {
                                                   p->setIrMode (v >= 0.5f
                                                                     ? CabinetProcessor::IrMode::stereo
                                                                     : CabinetProcessor::IrMode::blend);
                                               }));
        }

        registry.addEffect (std::move (e));
    }

    if (auto* p = chain.delay)
    {
        EffectDescriptor e;
        e.id = "delay" + suffix;
        e.label = instanced ("Delay");
        e.description = "Delay berumpan balik dengan waktu yang meluncur";
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };
        e.parameters.push_back (makeParam ("timeMs", "Time", "ms", 10.0f, 1000.0f, 1.0f, 350.0f,
                                           [p] { return p->getTimeMs(); },
                                           [p] (float v) { p->setTimeMs (v); }));
        e.parameters.push_back (makeParam ("feedbackPct", "Feedback", "%", 0.0f, 95.0f, 1.0f, 30.0f,
                                           [p] { return p->getFeedbackPercent(); },
                                           [p] (float v) { p->setFeedbackPercent (v); }));
        e.parameters.push_back (makeParam ("mixPct", "Mix", "%", 0.0f, 100.0f, 1.0f, 25.0f,
                                           [p] { return p->getMixPercent(); },
                                           [p] (float v) { p->setMixPercent (v); }));
        e.parameters.push_back (makeParam ("dampingHz", "Damping", "Hz", 500.0f, 20000.0f, 100.0f, 20000.0f,
                                           [p] { return p->getDampingHz(); },
                                           [p] (float v) { p->setDampingHz (v); }));
        e.parameters.push_back (makeToggle ("pingPong", "Ping-Pong", false,
                                            [p] { return p->isPingPong(); },
                                            [p] (bool v) { p->setPingPong (v); }));
        e.parameters.push_back (makeToggle ("spillover", "Spillover", true,
                                            [p] { return p->isSpillover(); },
                                            [p] (bool v) { p->setSpillover (v); }));
        // Enum 0..5 = Off / 1/4 / 1/8. / 1/8 / 1/8T / 1/16. Anything but Off
        // overrides the Time knob, which the UI shows by disabling it.
        e.parameters.push_back (makeParam ("syncMode", "Sync", "", 0.0f,
                                           (float) (DelayProcessor::kNumSyncDivisions - 1),
                                           1.0f, 0.0f,
                                           [p] { return (float) p->getSyncDivision(); },
                                           [p] (float v) { p->setSyncDivision ((int) std::lround (v)); }));
        registry.addEffect (std::move (e));
    }

    if (auto* p = chain.reverb)
    {
        EffectDescriptor e;
        e.id = "reverb" + suffix;
        e.label = instanced ("Reverb");
        e.description = "Ruang gema bergaya Freeverb";
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };
        e.parameters.push_back (makeParam ("roomSize", "Size", "", 0.0f, 1.0f, 0.01f, 0.5f,
                                           [p] { return p->getRoomSize(); },
                                           [p] (float v) { p->setRoomSize (v); }));
        e.parameters.push_back (makeParam ("decayTime", "Decay", "s", 0.2f, 10.0f, 0.1f, 2.0f,
                                           [p] { return p->getDecayTime(); },
                                           [p] (float v) { p->setDecayTime (v); }));
        e.parameters.push_back (makeParam ("width", "Width", "", 0.0f, 1.0f, 0.01f, 1.0f,
                                           [p] { return p->getWidth(); },
                                           [p] (float v) { p->setWidth (v); }));
        e.parameters.push_back (makeParam ("dryWetMix", "Mix", "", 0.0f, 1.0f, 0.01f, 0.25f,
                                           [p] { return p->getDryWetMix(); },
                                           [p] (float v) { p->setDryWetMix (v); }));
        e.parameters.push_back (makeToggle ("spillover", "Spillover", true,
                                            [p] { return p->isSpillover(); },
                                            [p] (bool v) { p->setSpillover (v); }));

        if (extras.irLibrary != nullptr)
        {
            e.parameters.push_back (makeToggle ("irEnabled", "Pakai IR", false,
                                                [p] { return p->isUsingImpulseResponse(); },
                                                [p] (bool v) { p->setUseImpulseResponse (v); }));
            e.parameters.push_back (makeIrFileParam (p, *extras.irLibrary,
                                                     milodikfx::preset::IrLibrary::Category::reverb));
        }

        registry.addEffect (std::move (e));
    }

    if (auto* p = chain.masterOut)
    {
        EffectDescriptor e;
        e.id = "master";
        e.label = "Master";
        e.description = "Level keluaran dan limiter pengaman";

        // Deliberately not toggleable. A header switch here looks exactly like
        // every other effect's bypass, but it would silence the whole app --
        // which is precisely how the output once ended up dead with no clue as
        // to why. Mute is an explicit, labelled control instead.
        e.isEnabled = [] { return true; };
        e.setEnabled = nullptr;

        e.parameters.push_back (makeToggle ("muted", "Mute", false,
                                            [p] { return p->isMuted(); },
                                            [p] (bool v) { p->setMuted (v); }));
        e.parameters.push_back (makeParam ("volumeDb", "Volume", "dB",
                                           MasterOutProcessor::kMinVolumeDb,
                                           MasterOutProcessor::kMaxVolumeDb,
                                           0.1f, 0.0f,
                                           [p] { return p->getVolumeDb(); },
                                           [p] (float v) { p->setVolumeDb (v); }));
        e.parameters.push_back (makeParam ("ceilingDb", "Ceiling", "dB", -24.0f, 0.0f, 0.1f, -0.3f,
                                           [p] { return p->getCeilingDb(); },
                                           [p] (float v) { p->setCeilingDb (v); }));
        e.parameters.push_back (makeToggle ("limiterEnabled", "Limiter", true,
                                            [p] { return p->isLimiterEnabled(); },
                                            [p] (bool v) { p->setLimiterEnabled (v); }));
        registry.addEffect (std::move (e));
    }

    if (auto* p = chain.metronome)
    {
        EffectDescriptor e;
        e.id = "metronome";
        e.label = "Metronome";
        e.description = "Klik latihan, dicampur setelah master (tidak lewat rantai efek)";

        // The stage itself is always present; "enabled" here means the click is
        // sounding, which is the metronome's own on/off rather than a bypass.
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };

        e.parameters.push_back (makeParam ("volumePct", "Volume", "%", 0.0f, 100.0f, 1.0f, 50.0f,
                                           [p] { return p->getVolumePercent(); },
                                           [p] (float v) { p->setVolumePercent (v); }));
        e.parameters.push_back (makeParam ("beatsPerBar", "Ketukan/Bar", "",
                                           1.0f, (float) MetronomeProcessor::kMaxBeatsPerBar, 1.0f, 4.0f,
                                           [p] { return (float) p->getBeatsPerBar(); },
                                           [p] (float v) { p->setBeatsPerBar ((int) std::lround (v)); }));
        registry.addEffect (std::move (e));
    }
}
} // namespace

void registerChainParameters (milodikfx::api::ParameterRegistry& registry,
                              const GuitarChain& chain,
                              DSPChainManager& manager,
                              ChainExtras extras)
{
    registerLayer (registry, chain, manager, extras, {}, true);

    // Instance 2 upwards. The suffix is the instance number, so the ids read
    // `overdrive2`, `overdrive3` -- and instance 1 keeps the bare id it has
    // always had, which is why nothing written before v0.31 needs migrating.
    for (size_t i = 0; i < chain.extras.size(); ++i)
        registerLayer (registry, chain.extras[i], manager, extras,
                       std::to_string (i + 2), false);
}
} // namespace milodikfx::dsp
