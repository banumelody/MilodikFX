#include "plugin/PluginProcessor.h"

#include <cmath>

#include "plugin/PluginBackend.h"
#include "plugin/PluginEditor.h"
#include "preset/UserPaths.h"

namespace milodikfx::plugin
{
namespace
{
constexpr const char* kStateTypeName = "MilodikFXState";

// Identifiers hold a String, so these are ordinary constants rather than
// constexpr ones. Note none of them may contain a dot: juce::Identifier rejects
// it, which is why the text parameters are child nodes with an effect/parameter
// pair rather than one "cabinet.irFile" property.
const juce::Identifier kTextParametersTag { "TextParameters" };
const juce::Identifier kTextEntryTag { "Text" };
const juce::Identifier kEffectProperty { "effect" };
const juce::Identifier kParameterProperty { "parameter" };
const juce::Identifier kValueProperty { "value" };
const juce::Identifier kPresetProperty { "preset" };
const juce::Identifier kLayoutProperty { "layout" };

/** How often the retired-model reaper runs and the latency figure is rechecked. */
constexpr int kHousekeepingMs = 500;

juce::String makeParameterId (const std::string& effectId, const std::string& parameterId)
{
    return juce::String (effectId) + "." + juce::String (parameterId);
}

juce::String makeEnabledId (const std::string& effectId)
{
    return juce::String (effectId) + ".enabled";
}

/**
 * A text parameter picks a file, so it cannot be a host parameter: VST3
 * automates numbers. They are carried in the plugin's own state instead and
 * driven from the editor.
 */
bool isHostAutomatable (const milodikfx::api::ParameterDescriptor& parameter) noexcept
{
    return ! parameter.isText && parameter.set != nullptr && parameter.get != nullptr;
}
} // namespace

MilodikFXAudioProcessor::MilodikFXAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      irLibrary (milodikfx::preset::UserPaths::impulseResponses()),
      namLibrary (milodikfx::preset::UserPaths::namModels()),
      presetManager (milodikfx::preset::UserPaths::presets())
{
    // No looper and no metronome: the host has both, and the looper would
    // allocate tens of megabytes per instance for controls that do not exist here.
    chain = milodikfx::dsp::buildGuitarChain (engine.getChain(), { false, false });

    // The libraries are what let a plugin instance load the same impulse
    // responses and amp captures the app uses. Without them those controls are
    // simply absent -- which is how this plugin shipped until now.
    milodikfx::dsp::ChainExtras extras;
    extras.irLibrary = &irLibrary;
    extras.namLibrary = &namLibrary;

    // No input-routing stage: the host decides what reaches us.
    milodikfx::dsp::registerChainParameters (registry, chain, engine.getChain(), extras);

    parameters = std::make_unique<juce::AudioProcessorValueTreeState> (
        *this, nullptr, juce::Identifier (kStateTypeName), buildLayout());

    buildBindings();

    bypassParameter = dynamic_cast<juce::AudioParameterBool*> (
        parameters->getParameter (makeParameterId ("global", "bypass")));

    applyAllBindings();

    startTimer (kHousekeepingMs);
}

MilodikFXAudioProcessor::~MilodikFXAudioProcessor()
{
    stopTimer();
    tunerAnalyzer.setEnabled (false);
}

PluginBackend& MilodikFXAudioProcessor::getBackend()
{
    // Built lazily so an instance that is never opened -- a whole project's
    // worth of them while rendering, say -- does not pay for scene, channel and
    // preset machinery nobody is going to look at.
    if (backend == nullptr)
    {
        backend = std::make_unique<PluginBackend> (*this);

        // Whatever the host restored before anyone opened the window.
        backend->applyLayout (pendingLayout);

        // Published only once the backend is fully built, so the audio thread
        // can never see a half-constructed handler.
        meters.store (&backend->getLevels(), std::memory_order_release);
    }

    return *backend;
}

void MilodikFXAudioProcessor::markStateChanged()
{
    updateHostDisplay (juce::AudioProcessorListener::ChangeDetails().withNonParameterStateChanged (true));
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MilodikFXAudioProcessor::buildLayout() const
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& effect : registry.getEffects())
    {
        if (effect.setEnabled)
            layout.add (std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID (makeEnabledId (effect.id), 1),
                juce::String (effect.label) + " On",
                effect.isEnabled ? effect.isEnabled() : true));

        for (const auto& parameter : effect.parameters)
        {
            if (! isHostAutomatable (parameter))
                continue;

            const auto id = makeParameterId (effect.id, parameter.id);
            const auto name = juce::String (effect.label) + " " + juce::String (parameter.label);

            if (parameter.isBoolean)
            {
                layout.add (std::make_unique<juce::AudioParameterBool> (
                    juce::ParameterID (id, 1), name, parameter.defaultValue >= 0.5f));
            }
            else
            {
                juce::NormalisableRange<float> range (parameter.minValue,
                                                      parameter.maxValue,
                                                      parameter.step > 0.0f ? parameter.step : 0.0001f);

                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID (id, 1),
                    name,
                    range,
                    parameter.defaultValue,
                    juce::AudioParameterFloatAttributes().withLabel (juce::String (parameter.unit))));
            }
        }
    }

    return layout;
}

void MilodikFXAudioProcessor::buildBindings()
{
    bindings.clear();

    // Resolved once. Everything the audio thread later needs is a pointer, so
    // applying an automated value never looks anything up by name.
    const auto resolve = [this] (const juce::String& id) -> std::pair<juce::RangedAudioParameter*, std::atomic<float>*>
    {
        return { parameters->getParameter (id), parameters->getRawParameterValue (id) };
    };

    for (const auto& effect : registry.getEffects())
    {
        if (effect.setEnabled)
        {
            const auto [hostParameter, source] = resolve (makeEnabledId (effect.id));

            if (hostParameter != nullptr && source != nullptr)
                bindings.push_back ({ hostParameter, source, source->load(), nullptr, &effect });
        }

        for (const auto& parameter : effect.parameters)
        {
            if (! isHostAutomatable (parameter))
                continue;

            const auto [hostParameter, source] = resolve (makeParameterId (effect.id, parameter.id));

            if (hostParameter != nullptr && source != nullptr)
                bindings.push_back ({ hostParameter, source, source->load(), &parameter, nullptr });
        }
    }
}

void MilodikFXAudioProcessor::applyAllBindings()
{
    for (auto& binding : bindings)
    {
        if (binding.source == nullptr)
            continue;

        const auto value = binding.source->load();
        binding.last = value;

        if (binding.parameter != nullptr && binding.parameter->set)
            binding.parameter->set (juce::jlimit (binding.parameter->minValue, binding.parameter->maxValue, value));
        else if (binding.effect != nullptr && binding.effect->setEnabled)
            binding.effect->setEnabled (value >= 0.5f);
    }
}

void MilodikFXAudioProcessor::pollHostParameters() noexcept
{
    // Polled rather than listened to. A parameter listener fires on whichever
    // thread the host automates from -- including the audio thread -- and the
    // old implementation answered it with a linear scan of string comparisons.
    // Reading the atomics once per block is a fixed cost with no strings in it,
    // and it removes the question of which thread this is running on entirely.
    for (auto& binding : bindings)
    {
        if (binding.source == nullptr)
            continue;

        const auto value = binding.source->load (std::memory_order_relaxed);

        if (value == binding.last)
            continue;

        binding.last = value;

        if (binding.parameter != nullptr)
            binding.parameter->set (juce::jlimit (binding.parameter->minValue,
                                                  binding.parameter->maxValue,
                                                  value));
        else if (binding.effect != nullptr)
            binding.effect->setEnabled (value >= 0.5f);
    }
}

void MilodikFXAudioProcessor::followHostTempo() noexcept
{
    // A synced delay and a tempo-locked LFO have to land on the project grid,
    // not on a number typed into this plugin. When the host reports nothing --
    // a bare standalone wrapper, or a host between transport states -- whatever
    // the Tempo parameter last said stands.
    auto* host = getPlayHead();

    if (host == nullptr || chain.delay == nullptr)
        return;

    const auto position = host->getPosition();

    if (! position.hasValue())
        return;

    const auto hostBpm = position->getBpm();

    if (! hostBpm.hasValue())
        return;

    const auto value = (float) *hostBpm;

    if (! std::isfinite (value) || value <= 0.0f)
        return;

    const auto clamped = juce::jlimit (milodikfx::dsp::MetronomeProcessor::kMinBpm,
                                       milodikfx::dsp::MetronomeProcessor::kMaxBpm,
                                       value);

    if (std::abs (clamped - lastHostBpm) < 0.001f)
        return;

    lastHostBpm = clamped;
    chain.delay->setBpm (clamped);
}

//==============================================================================
void MilodikFXAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto channels = juce::jmax (1, getTotalNumOutputChannels());

    engine.prepareToPlay (sampleRate, samplesPerBlock, channels);

    lastHostBpm = 0.0f;

    reportedLatency = computeLatencySamples();
    setLatencySamples (reportedLatency);
}

void MilodikFXAudioProcessor::releaseResources()
{
    engine.reset();
}

bool MilodikFXAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    const auto& in = layouts.getMainInputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    // Mono in / stereo out is the normal way to plug a guitar into a DAW.
    if (in != out && in != juce::AudioChannelSet::mono())
        return false;

    return true;
}

void MilodikFXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const juce::ScopedNoDenormals noDenormals;

    pollHostParameters();
    followHostTempo();

    const auto totalIn = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    // A mono source must reach both sides before the chain runs, otherwise the
    // guitar ends up in one ear.
    if (totalIn == 1 && totalOut > 1)
        for (int ch = 1; ch < totalOut; ++ch)
            buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);

    // Tapped before the chain: pitch detection has to see the raw pickup. A
    // no-op unless the tuner panel is actually open, and even then the audio
    // thread only copies into a ring buffer -- YIN runs on a worker.
    if (numSamples > 0 && buffer.getNumChannels() > 0)
        tunerAnalyzer.pushSamples (buffer.getReadPointer (0), numSamples);

    const auto inputPeak = buffer.getMagnitude (0, numSamples);

    engine.processBlock (buffer);

    // Meters for the editor, through the same handler the app's HTTP layer uses.
    // Plain relaxed stores; nothing here blocks or allocates.
    if (auto* levels = meters.load (std::memory_order_relaxed))
    {
        const auto outputPeak = buffer.getMagnitude (0, numSamples);

        levels->updateLevels (juce::Decibels::gainToDecibels (inputPeak, kMeterFloorDb),
                              juce::Decibels::gainToDecibels (inputPeak, kMeterFloorDb),
                              juce::Decibels::gainToDecibels (outputPeak, kMeterFloorDb));

        levels->updateGainReduction (chain.noiseGate != nullptr ? chain.noiseGate->getCurrentGain() : 1.0f,
                                     chain.compressor != nullptr ? chain.compressor->getGainReductionDb() : 0.0f,
                                     chain.masterOut != nullptr ? chain.masterOut->getLimiterReductionDb() : 0.0f);

        if (const auto rate = getSampleRate(); rate > 0.0)
            levels->updateLoad (0.0f, rate, numSamples);

        levels->setAudioRunning (true);
    }
}

int MilodikFXAudioProcessor::computeLatencySamples() const
{
    int latency = 0;

    if (chain.overdrive != nullptr)
        latency += (int) std::lround (chain.overdrive->getLatencySamples());

    // Measured at load time by pushing an impulse through the round trip, not
    // estimated -- and zero when the session rate already matches the model's,
    // which is why a 48 kHz project adds nothing here at all.
    if (chain.nam != nullptr)
        latency += chain.nam->getLatencySamples();

    return juce::jmax (0, latency);
}

void MilodikFXAudioProcessor::timerCallback()
{
    // Reap a model the audio thread retired. The reference NAM plugin frees it
    // inside its audio callback; this collects it on the message thread instead.
    if (chain.nam != nullptr)
        chain.nam->collectGarbage();

    // Loading a model, or changing the drive's oversampling, moves the figure
    // the host has to compensate by. Reporting it only at prepare time left a
    // NAM instance mis-aligned against every other track in the project.
    const auto latency = computeLatencySamples();

    if (latency != reportedLatency)
    {
        reportedLatency = latency;
        setLatencySamples (latency);
    }
}

//==============================================================================
juce::AudioProcessorEditor* MilodikFXAudioProcessor::createEditor()
{
    return new MilodikFXAudioProcessorEditor (*this);
}

juce::ValueTree MilodikFXAudioProcessor::captureTextParameters() const
{
    juce::ValueTree tree (kTextParametersTag);

    for (const auto& effect : registry.getEffects())
    {
        for (const auto& parameter : effect.parameters)
        {
            if (! parameter.isText || ! parameter.getText)
                continue;

            const auto value = parameter.getText();

            if (value.isEmpty())
                continue;

            juce::ValueTree entry (kTextEntryTag);
            entry.setProperty (kEffectProperty, juce::String (effect.id), nullptr);
            entry.setProperty (kParameterProperty, juce::String (parameter.id), nullptr);
            entry.setProperty (kValueProperty, value, nullptr);
            tree.appendChild (entry, nullptr);
        }
    }

    return tree;
}

void MilodikFXAudioProcessor::applyTextParameters (const juce::ValueTree& tree)
{
    if (! tree.isValid())
        return;

    // Reads files, so message thread only -- which is where a host restores
    // state. The same rule the REST layer follows: never the audio thread.
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const auto entry = tree.getChild (i);

        juce::String applied;
        registry.setTextParameter (entry[kEffectProperty].toString().toStdString(),
                                   entry[kParameterProperty].toString().toStdString(),
                                   entry[kValueProperty].toString(),
                                   applied);
    }
}

void MilodikFXAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters->copyState();

    if (! state.isValid())
        return;

    // copyState hands back a deep copy, so the extras below never touch what the
    // APVTS is actually using.
    state.removeChild (state.getChildWithName (kTextParametersTag), nullptr);
    state.appendChild (captureTextParameters(), nullptr);
    state.setProperty (kPresetProperty, currentPreset, nullptr);

    // Scenes, channels and pins. An instance whose window was never opened has
    // no backend, so its layout is written back exactly as it was restored --
    // never silently dropped just because nobody looked at it.
    const auto layout = backend != nullptr ? backend->captureLayout() : pendingLayout;

    if (layout.isObject())
        state.setProperty (kLayoutProperty, juce::JSON::toString (layout, true), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MilodikFXAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (kStateTypeName))
        return;

    const auto state = juce::ValueTree::fromXml (*xml);

    if (! state.isValid())
        return;

    parameters->replaceState (state);

    // replaceState only moves the host parameters. The chain still holds the old
    // values until they are pushed through, and the poll would not do it: it
    // fires on change, and `last` was seeded from the values being replaced.
    applyAllBindings();
    applyTextParameters (state.getChildWithName (kTextParametersTag));

    currentPreset = state[kPresetProperty].toString();

    pendingLayout = juce::var();
    juce::JSON::parse (state[kLayoutProperty].toString(), pendingLayout);

    // Applied now when the window is already open, and at construction otherwise.
    if (backend != nullptr)
        backend->applyLayout (pendingLayout);
}

//==============================================================================
bool MilodikFXAudioProcessor::loadPreset (const juce::String& name)
{
    juce::var state;

    if (! presetManager.loadPreset (name, state))
        return false;

    // Straight into the chain through the registry setters -- the atomics a MIDI
    // CC writes, plus setText for the impulse response and the amp model, which
    // read their files here on the message thread. Then the host is told what
    // the values became, or its automation lanes would still hold the old ones.
    registry.applyState (state);

    currentPreset = name;
    syncHostParametersFromChain();

    return true;
}

bool MilodikFXAudioProcessor::savePreset (const juce::String& name)
{
    if (! presetManager.savePreset (name, registry.captureState()))
        return false;

    currentPreset = name;
    return true;
}

bool MilodikFXAudioProcessor::setTextParameter (const juce::String& effectId,
                                                const juce::String& parameterId,
                                                const juce::String& value)
{
    juce::String applied;

    return registry.setTextParameter (effectId.toStdString(), parameterId.toStdString(), value, applied);
}

void MilodikFXAudioProcessor::syncHostParametersFromChain()
{
    // Message thread only: setValueNotifyingHost is what tells the host that a
    // value it did not move has changed, and it must not be called from audio.
    JUCE_ASSERT_MESSAGE_THREAD

    for (auto& binding : bindings)
    {
        if (binding.hostParameter == nullptr)
            continue;

        float current = 0.0f;

        if (binding.parameter != nullptr && binding.parameter->get)
            current = binding.parameter->get();
        else if (binding.effect != nullptr && binding.effect->isEnabled)
            current = binding.effect->isEnabled() ? 1.0f : 0.0f;
        else
            continue;

        // Seed `last` first: the poll must not read this back as a host move and
        // write it straight into the chain again.
        binding.last = current;
        binding.hostParameter->setValueNotifyingHost (binding.hostParameter->convertTo0to1 (current));
    }
}
} // namespace milodikfx::plugin

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new milodikfx::plugin::MilodikFXAudioProcessor();
}
