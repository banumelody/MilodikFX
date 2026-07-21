#pragma once

#include <JuceHeader.h>

#include <memory>
#include <vector>

#include "api/ParameterRegistry.h"
#include "audio/AudioEngine.h"
#include "dsp/ChainFactory.h"

namespace milodikfx::plugin
{
/**
 * The MilodikFX chain as a plugin.
 *
 * Shares src/dsp with the standalone app through ChainFactory, so the two can
 * never drift apart, and exposes every registered parameter to the host through
 * an AudioProcessorValueTreeState built from the same descriptors the REST API
 * and the UI use.
 */
class MilodikFXAudioProcessor final : public juce::AudioProcessor,
                                      private juce::AudioProcessorValueTreeState::Listener
{
public:
    MilodikFXAudioProcessor();
    ~MilodikFXAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return *parameters; }

private:
    /** Maps a host parameter id onto the registry entry it drives. */
    struct Binding
    {
        juce::String parameterId;
        std::function<void (float)> apply;
    };

    void parameterChanged (const juce::String& parameterID, float newValue) override;
    juce::AudioProcessorValueTreeState::ParameterLayout buildLayout();
    void pushAllParametersToChain();

    milodikfx::audio::AudioEngine engine;
    milodikfx::dsp::GuitarChain chain;
    milodikfx::api::ParameterRegistry registry;

    std::unique_ptr<juce::AudioProcessorValueTreeState> parameters;

    // Looked up on the audio thread when a host automates a value, so it holds
    // no allocation and no string building.
    std::vector<Binding> bindings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MilodikFXAudioProcessor)
};
} // namespace milodikfx::plugin
