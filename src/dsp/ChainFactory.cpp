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
} // namespace

GuitarChain buildGuitarChain (DSPChainManager& chain)
{
    GuitarChain result;

    // Signal order matters: gate the raw pickup before anything boosts its
    // noise, compress before the clipper so the drive sees a steady level, and
    // put the cabinet after all the distortion it is meant to be filtering.
    result.noiseGate = add<NoiseGateProcessor> (chain);
    result.cleanBoost = add<GainProcessor> (chain);
    result.compressor = add<CompressorProcessor> (chain);
    result.overdrive = add<OverdriveProcessor> (chain);
    result.eq = add<EQProcessor> (chain);
    result.toneStack = add<ToneStackProcessor> (chain);
    result.cabinet = add<CabinetProcessor> (chain);
    result.delay = add<DelayProcessor> (chain);
    result.reverb = add<ReverbProcessor> (chain);
    result.masterOut = add<MasterOutProcessor> (chain);

    return result;
}

void registerChainParameters (milodikfx::api::ParameterRegistry& registry,
                              const GuitarChain& chain,
                              std::function<float()> getInputMode,
                              std::function<void (float)> setInputMode)
{
    // Effect and parameter ids double as settings keys (dsp.<effect>.<param>)
    // and as REST path segments, so they stay stable even when labels change.
    if (getInputMode && setInputMode)
    {
        EffectDescriptor e;
        e.id = "input";
        e.label = "Input";
        e.description = "How the interface's input channels feed the chain";
        e.isEnabled = [] { return true; };
        e.setEnabled = [] (bool) {};
        e.parameters.push_back (makeParam ("mode", "Mode", "", 0.0f, 3.0f, 1.0f, 0.0f,
                                           std::move (getInputMode), std::move (setInputMode)));
        registry.addEffect (std::move (e));
    }

    if (auto* p = chain.noiseGate)
    {
        EffectDescriptor e;
        e.id = "noiseGate";
        e.label = "Noise Gate";
        e.description = "Silences pickup hum between notes";
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
        e.description = "Lifts a weak pickup before the rest of the chain";
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
        e.description = "Evens out picking dynamics";
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };
        e.parameters.push_back (makeParam ("inputGainDb", "Input", "dB", -24.0f, 24.0f, 0.1f, 0.0f,
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
        e.description = "Cubic soft clipper, oversampled";
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };
        e.parameters.push_back (makeParam ("drivePct", "Drive", "%", 0.0f, 100.0f, 0.5f, 0.0f,
                                           [p] { return p->getDrivePercent(); },
                                           [p] (float v) { p->setDrivePercent (v); }));
        e.parameters.push_back (makeParam ("levelPct", "Level", "%", 0.0f, 100.0f, 0.5f, 100.0f,
                                           [p] { return p->getLevelPercent(); },
                                           [p] (float v) { p->setLevelPercent (v); }));
        e.parameters.push_back (makeToggle ("oversampling", "Oversampling", true,
                                            [p] { return p->isOversamplingEnabled(); },
                                            [p] (bool v) { p->setOversamplingEnabled (v); }));
        registry.addEffect (std::move (e));
    }

    if (auto* p = chain.eq)
    {
        EffectDescriptor e;
        e.id = "eq";
        e.label = "EQ";
        e.description = "120 Hz shelf / 1 kHz peak / 7 kHz shelf";
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
        e.description = "50 Hz / 500 Hz / 5 kHz shaping";
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

    if (auto* p = chain.cabinet)
    {
        EffectDescriptor e;
        e.id = "cabinet";
        e.label = "Cabinet";
        e.description = "Speaker emulation - leave this on for a DI'd guitar";
        e.isEnabled = [p] { return p->isEnabled(); };
        e.setEnabled = [p] (bool v) { p->setEnabled (v); };
        e.parameters.push_back (makeParam ("presenceDb", "Presence", "dB", -12.0f, 12.0f, 0.1f, 0.0f,
                                           [p] { return p->getPresenceDb(); },
                                           [p] (float v) { p->setPresenceDb (v); }));
        e.parameters.push_back (makeParam ("toneHz", "Tone", "Hz", 2000.0f, 8000.0f, 50.0f, 5500.0f,
                                           [p] { return p->getToneHz(); },
                                           [p] (float v) { p->setToneHz (v); }));
        registry.addEffect (std::move (e));
    }

    if (auto* p = chain.delay)
    {
        EffectDescriptor e;
        e.id = "delay";
        e.label = "Delay";
        e.description = "Feedback delay with a gliding time control";
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
        registry.addEffect (std::move (e));
    }

    if (auto* p = chain.reverb)
    {
        EffectDescriptor e;
        e.id = "reverb";
        e.label = "Reverb";
        e.description = "Freeverb-style room";
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
        registry.addEffect (std::move (e));
    }

    if (auto* p = chain.masterOut)
    {
        EffectDescriptor e;
        e.id = "master";
        e.label = "Master";
        e.description = "Output level and safety limiter";
        // Switching the master "off" mutes rather than bypasses, so the limiter
        // can never be taken out of the signal path.
        e.isEnabled = [p] { return ! p->isMuted(); };
        e.setEnabled = [p] (bool v) { p->setMuted (! v); };
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
}
} // namespace milodikfx::dsp
