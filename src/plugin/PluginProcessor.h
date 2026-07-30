#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <memory>
#include <vector>

#include "api/LevelsHandler.h"
#include "api/ParameterRegistry.h"
#include "audio/AudioEngine.h"
#include "dsp/ChainFactory.h"
#include "dsp/TunerAnalyzer.h"
#include "preset/IrLibrary.h"
#include "preset/NamLibrary.h"
#include "preset/PresetManager.h"

namespace milodikfx::plugin
{
class PluginBackend;

/**
 * The MilodikFX chain as a plugin.
 *
 * Shares src/dsp with the standalone app through ChainFactory, so the two can
 * never drift apart, and exposes every registered parameter to the host through
 * an AudioProcessorValueTreeState built from the same descriptors the REST API
 * and the UI use.
 *
 * Three things differ from the app, and each is deliberate:
 *
 * - **No looper, no metronome.** A host has both already, and the looper is not
 *   free to carry: it allocates its whole 60-second record buffer at prepare
 *   time, tens of megabytes per instance, for a feature with no controls here.
 * - **The host owns the tempo.** When the playhead reports a BPM it is pushed
 *   into the chain every block, so a synced delay lands on the project grid
 *   rather than on a number typed into this plugin.
 * - **The host owns persistence.** State goes through get/setStateInformation;
 *   nothing writes a settings file behind the host's back. Presets are still
 *   read from the same folder the app uses, so a sound built on stage opens in
 *   the studio.
 */
class MilodikFXAudioProcessor final : public juce::AudioProcessor,
                                      private juce::Timer
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

    /** Long enough for the slowest reverb decay plus a delay tail to run out. */
    double getTailLengthSeconds() const override { return 12.0; }

    /**
     * Hands the host the chain's own bypass switch, so its bypass button gets
     * the 10 ms crossfade the engine already does instead of a hard cut.
     */
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParameter; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return *parameters; }
    milodikfx::api::ParameterRegistry& getRegistry() noexcept { return registry; }

    milodikfx::preset::PresetManager& getPresetManager() noexcept { return presetManager; }
    milodikfx::preset::IrLibrary& getIrLibrary() noexcept { return irLibrary; }
    milodikfx::preset::NamLibrary& getNamLibrary() noexcept { return namLibrary; }
    milodikfx::dsp::TunerAnalyzer& getTuner() noexcept { return tunerAnalyzer; }

    /** The API surface the editor's WebView talks to. Built on first use. */
    PluginBackend& getBackend();

    /**
     * Something the UI changed that the host cannot see in a parameter -- a
     * preset selection, a scene, a pinned knob. Tells the host its project has
     * unsaved changes.
     */
    void markStateChanged();

    void setCurrentPresetName (const juce::String& name) { currentPreset = name; }

    //==============================================================================
    /** @name Presets — the same .json files the standalone app reads. */
    /** @{ */
    juce::StringArray listPresets() const { return presetManager.listPresets(); }
    bool loadPreset (const juce::String& name);
    bool savePreset (const juce::String& name);
    juce::String getCurrentPresetName() const { return currentPreset; }
    /** @} */

    /**
     * Writes a text parameter (an impulse response or a NAM model) and reads the
     * file on the calling thread. Message thread only — never the audio thread.
     */
    bool setTextParameter (const juce::String& effectId,
                           const juce::String& parameterId,
                           const juce::String& value);

    /** Re-reads every value from the chain and tells the host about it. */
    void syncHostParametersFromChain();

private:
    /**
     * One host parameter wired to the registry entry it drives.
     *
     * The descriptor pointer is resolved once, at construction: `set` is the
     * plain atomic store a MIDI CC does, so applying an automated value costs
     * no lookup, no string compare and no allocation on the audio thread. The
     * registry is never added to after construction, so the pointer is stable
     * for the life of the processor.
     */
    struct Binding
    {
        juce::RangedAudioParameter* hostParameter = nullptr;
        std::atomic<float>* source = nullptr;
        float last = 0.0f;

        const milodikfx::api::ParameterDescriptor* parameter = nullptr;
        const milodikfx::api::EffectDescriptor* effect = nullptr;
    };

    juce::AudioProcessorValueTreeState::ParameterLayout buildLayout() const;
    void buildBindings();
    void applyAllBindings();

    /** Applies whatever the host has moved since the last block. */
    void pollHostParameters() noexcept;

    /** Follows the playhead's tempo when it reports one. */
    void followHostTempo() noexcept;

    int computeLatencySamples() const;
    void timerCallback() override;

    juce::ValueTree captureTextParameters() const;
    void applyTextParameters (const juce::ValueTree& tree);

    // Declared before the chain registration that captures references to them.
    milodikfx::preset::IrLibrary irLibrary;
    milodikfx::preset::NamLibrary namLibrary;
    milodikfx::preset::PresetManager presetManager;

    milodikfx::audio::AudioEngine engine;
    milodikfx::dsp::GuitarChain chain;
    milodikfx::api::ParameterRegistry registry;

    // Not a chain stage: the input buffer is tapped before the chain runs,
    // because pitch detection has to see the raw pickup rather than a clipped,
    // cabinet-filtered version whose harmonics would fool it.
    milodikfx::dsp::TunerAnalyzer tunerAnalyzer;

    std::unique_ptr<juce::AudioProcessorValueTreeState> parameters;
    std::unique_ptr<PluginBackend> backend;
    std::vector<Binding> bindings;

    /**
     * Where the audio thread posts its meter readings, or null while no editor
     * has ever been opened. Published with release once the backend exists and
     * never cleared -- the backend outlives every editor, so the audio thread
     * can read it without any lifetime question at all.
     */
    std::atomic<LevelsHandler*> meters { nullptr };

    /** Silence floor, matching the app's meters. */
    static constexpr float kMeterFloorDb = -100.0f;

    juce::AudioParameterBool* bypassParameter = nullptr;

    juce::String currentPreset;

    /**
     * Scenes, channels and pins as the host last restored them, held until the
     * backend exists to receive them -- an instance nobody opened still has to
     * save back what it was given.
     */
    juce::var pendingLayout;

    // Written by the audio thread, read by the timer that reports latency, so a
    // model load changes the figure the host compensates by.
    int reportedLatency = -1;

    // The tempo last pushed into the chain, so a steady host does not restate
    // it every block.
    float lastHostBpm = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MilodikFXAudioProcessor)
};
} // namespace milodikfx::plugin
