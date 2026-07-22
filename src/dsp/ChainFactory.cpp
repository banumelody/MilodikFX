#include "dsp/ChainFactory.h"

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

GuitarChain buildGuitarChain (DSPChainManager& chain)
{
    GuitarChain result;

    // Signal order matters: trim the guitar to the chain before the gate sees
    // it, so the gate threshold stays correct relative to the signal; gate the
    // raw pickup before anything boosts its noise; compress before the clipper
    // so the drive sees a steady level; and put the cabinet after all the
    // distortion it is meant to be filtering.
    result.inputTrim = add<InputTrimProcessor> (chain);
    result.noiseGate = add<NoiseGateProcessor> (chain);
    result.cleanBoost = add<GainProcessor> (chain);
    result.compressor = add<CompressorProcessor> (chain);
    result.overdrive = add<OverdriveProcessor> (chain);
    result.eq = add<EQProcessor> (chain);
    result.toneStack = add<ToneStackProcessor> (chain);
    // The amp head sits between the tone shaping and the cabinet, exactly where
    // a real head sits between the pedals and the speaker.
    result.nam = add<NamProcessor> (chain);
    result.cabinet = add<CabinetProcessor> (chain);
    result.delay = add<DelayProcessor> (chain);
    result.reverb = add<ReverbProcessor> (chain);
    result.masterOut = add<MasterOutProcessor> (chain);

    // Not a stage of the chain: mixed in afterwards so bypass cannot silence it.
    result.metronome = dynamic_cast<MetronomeProcessor*> (
        chain.addPostProcessor (std::make_unique<MetronomeProcessor>()));

    return result;
}

void registerChainParameters (milodikfx::api::ParameterRegistry& registry,
                              const GuitarChain& chain,
                              DSPChainManager& manager,
                              ChainExtras extras)
{
    auto getInputMode = std::move (extras.getInputMode);
    auto setInputMode = std::move (extras.setInputMode);

    // Global controls that belong to the chain as a whole rather than to any one
    // effect. Always in the path, so never toggleable as a unit.
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

        // One tempo for the whole app. The metronome stores it and the delay is
        // handed a copy, so a synced repeat always lands on the click rather
        // than drifting against a second, separately-edited BPM.
        if (auto* metronome = chain.metronome)
        {
            auto* delay = chain.delay;

            e.parameters.push_back (makeParam ("bpm", "Tempo", "BPM",
                                               MetronomeProcessor::kMinBpm,
                                               MetronomeProcessor::kMaxBpm,
                                               1.0f, 120.0f,
                                               [metronome] { return metronome->getBpm(); },
                                               [metronome, delay] (float v)
                                               {
                                                   metronome->setBpm (v);

                                                   if (delay != nullptr)
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
        e.id = "noiseGate";
        e.label = "Noise Gate";
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
        e.id = "cleanBoost";
        e.label = "Clean Boost";
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
        e.id = "compressor";
        e.label = "Compressor";
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

    if (auto* p = chain.overdrive)
    {
        EffectDescriptor e;
        e.id = "overdrive";
        e.label = "Overdrive";
        e.description = "Pilih voicing pedalnya - kontrol menyesuaikan tipe";
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
        e.id = "eq";
        e.label = "EQ";
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
        e.id = "toneStack";
        e.label = "Contour";
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
        e.id = "cabinet";
        e.label = "Cabinet";
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
        }

        registry.addEffect (std::move (e));
    }

    if (auto* p = chain.delay)
    {
        EffectDescriptor e;
        e.id = "delay";
        e.label = "Delay";
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
        e.id = "reverb";
        e.label = "Reverb";
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
} // namespace milodikfx::dsp
