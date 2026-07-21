#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <functional>

#include "api/ParameterRegistry.h"

namespace milodikfx::midi
{
/** What a control change does when it arrives. */
enum class MappingMode
{
    /** CC 0..127 spread across the parameter's range. For knobs and pedals. */
    continuous = 0,

    /** Any press flips the value. One stomp, one toggle -- footswitches send
        127 on press and 0 on release, and a continuous mapping would make you
        hold the switch down to keep the effect on. */
    toggle = 1
};

struct Mapping
{
    juce::String effectId;
    juce::String parameterId;
    MappingMode mode = MappingMode::continuous;

    bool isValid() const noexcept { return effectId.isNotEmpty() && parameterId.isNotEmpty(); }
};

/**
 * MIDI input: continuous controls, footswitches, and preset changes.
 *
 * Owns every interaction with juce::MidiInput and marshals device open/close to
 * the message thread, the same discipline AudioDeviceController follows -- REST
 * handlers run on arbitrary Winsock threads.
 *
 * Incoming messages arrive on JUCE's MIDI thread. Parameter writes from there
 * are safe because every processor holds its parameters as atomics; anything
 * that touches files or the device (loading a preset) is posted to the message
 * thread instead.
 */
class MidiController final : public juce::MidiInputCallback
{
public:
    explicit MidiController (milodikfx::api::ParameterRegistry& registryToUse);
    ~MidiController() override;

    static constexpr int kNumControllers = 128;

    /** Names of every MIDI input currently attached. */
    static juce::StringArray listDevices();

    /** Opens an input by name. An empty name closes whatever is open.
        Returns an empty string on success, otherwise the reason. */
    juce::String openDevice (const juce::String& deviceName);

    juce::String getOpenDeviceName() const;
    bool isOpen() const;

    void setMapping (int ccNumber, const Mapping& mapping);
    void clearMapping (int ccNumber);
    Mapping getMapping (int ccNumber) const;

    /** Every mapped controller, lowest CC first. */
    std::vector<std::pair<int, Mapping>> getMappings() const;

    /**
     * Arms MIDI learn: the next control change binds to this parameter.
     *
     * Far kinder than typing CC numbers into a table -- you touch the control
     * you want and it is bound. Pass an invalid mapping to disarm.
     */
    void startLearning (const Mapping& target);
    void stopLearning();
    bool isLearning() const;
    Mapping getLearnTarget() const;

    /** The last control change seen, for the UI to echo. -1 when there is none. */
    int getLastControllerNumber() const noexcept { return lastCc.load (std::memory_order_relaxed); }
    int getLastControllerValue() const noexcept { return lastCcValue.load (std::memory_order_relaxed); }

    /** Fired on the message thread when a program change selects a preset. */
    std::function<void (int programNumber)> onProgramChange;

    /** Fired on the message thread after a mapped parameter has been written. */
    std::function<void()> onParameterChanged;

    /** Fired on the message thread when learn mode binds a controller. */
    std::function<void (int ccNumber, Mapping)> onLearned;

    /**
     * Fired whenever the mapping table or the open device changes.
     *
     * Without this a mapping only reached the settings file if something else
     * happened to have marked them dirty -- so bindings survived a clean quit
     * and vanished on anything else.
     */
    std::function<void()> onConfigurationChanged;

    /**
     * Where JUCE delivers incoming messages. Public because it is the only way
     * to exercise the dispatch without a physical controller attached, and a
     * build machine cannot be assumed to have one.
     */
    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;

private:
    void applyControlChange (int ccNumber, int value);

    milodikfx::api::ParameterRegistry& registry;

    // Guards the mapping table and the learn target. Held only long enough to
    // copy one entry out; the MIDI thread is high priority but not realtime, so
    // a short lock there is fine where it would not be on the audio thread.
    mutable juce::CriticalSection lock;

    std::array<Mapping, kNumControllers> mappings;
    Mapping learnTarget;

    std::unique_ptr<juce::MidiInput> input;
    juce::String openName;

    std::atomic<int> lastCc { -1 };
    std::atomic<int> lastCcValue { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiController)
};
} // namespace milodikfx::midi
