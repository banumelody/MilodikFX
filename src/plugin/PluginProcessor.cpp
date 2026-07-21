#include "plugin/PluginProcessor.h"

namespace milodikfx::plugin
{
namespace
{
constexpr const char* kStateTypeName = "MilodikFXState";

juce::String makeParameterId (const std::string& effectId, const std::string& parameterId)
{
    return juce::String (effectId) + "." + juce::String (parameterId);
}

juce::String makeEnabledId (const std::string& effectId)
{
    return juce::String (effectId) + ".enabled";
}
} // namespace

MilodikFXAudioProcessor::MilodikFXAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    chain = milodikfx::dsp::buildGuitarChain (engine.getChain());

    // No input-routing stage: the host decides what reaches us.
    milodikfx::dsp::registerChainParameters (registry, chain);

    parameters = std::make_unique<juce::AudioProcessorValueTreeState> (
        *this, nullptr, juce::Identifier (kStateTypeName), buildLayout());

    for (const auto& binding : bindings)
        parameters->addParameterListener (binding.parameterId, this);

    pushAllParametersToChain();
}

MilodikFXAudioProcessor::~MilodikFXAudioProcessor()
{
    if (parameters != nullptr)
        for (const auto& binding : bindings)
            parameters->removeParameterListener (binding.parameterId, this);
}

juce::AudioProcessorValueTreeState::ParameterLayout MilodikFXAudioProcessor::buildLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& effect : registry.getEffects())
    {
        if (effect.setEnabled)
        {
            const auto id = makeEnabledId (effect.id);
            const auto effectId = effect.id;

            layout.add (std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID (id, 1),
                juce::String (effect.label) + " On",
                effect.isEnabled ? effect.isEnabled() : true));

            bindings.push_back ({ id,
                                  [this, effectId] (float value)
                                  {
                                      registry.setEffectEnabled (effectId, value >= 0.5f);
                                  } });
        }

        for (const auto& parameter : effect.parameters)
        {
            const auto id = makeParameterId (effect.id, parameter.id);
            const auto effectId = effect.id;
            const auto parameterId = parameter.id;
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

            bindings.push_back ({ id,
                                  [this, effectId, parameterId] (float value)
                                  {
                                      float applied = 0.0f;
                                      registry.setParameter (effectId, parameterId, value, applied);
                                  } });
        }
    }

    return layout;
}

void MilodikFXAudioProcessor::pushAllParametersToChain()
{
    for (const auto& binding : bindings)
        if (auto* raw = parameters->getRawParameterValue (binding.parameterId))
            binding.apply (raw->load());
}

void MilodikFXAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    // Called on whichever thread the host automates from, including the audio
    // thread, so this only touches atomics behind the registry's setters.
    for (const auto& binding : bindings)
    {
        if (binding.parameterId == parameterID)
        {
            binding.apply (newValue);
            return;
        }
    }
}

void MilodikFXAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto channels = juce::jmax (1, getTotalNumOutputChannels());

    engine.prepareToPlay (sampleRate, samplesPerBlock, channels);

    if (chain.overdrive != nullptr)
        setLatencySamples ((int) std::lround (chain.overdrive->getLatencySamples()));
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

    const auto totalIn = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    // A mono source must reach both sides before the chain runs, otherwise the
    // guitar ends up in one ear.
    if (totalIn == 1 && totalOut > 1)
        for (int ch = 1; ch < totalOut; ++ch)
            buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);

    engine.processBlock (buffer);
}

juce::AudioProcessorEditor* MilodikFXAudioProcessor::createEditor()
{
    // The rich UI belongs to the standalone app, which owns the local server it
    // is served from. In a host, the generic editor exposes every parameter
    // without a plugin instance trying to open a socket.
    return new juce::GenericAudioProcessorEditor (*this);
}

void MilodikFXAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = parameters->copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void MilodikFXAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (kStateTypeName))
            parameters->replaceState (juce::ValueTree::fromXml (*xml));

    pushAllParametersToChain();
}
} // namespace milodikfx::plugin

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new milodikfx::plugin::MilodikFXAudioProcessor();
}
