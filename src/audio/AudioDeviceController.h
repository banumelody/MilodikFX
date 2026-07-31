#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace milodikfx::audio
{
/** Everything the UI needs to know about the device that is actually open. */
struct AudioDeviceSnapshot
{
    bool isOpen = false;
    juce::String typeName;
    juce::String inputDeviceName;
    juce::String outputDeviceName;
    double sampleRate = 0.0;
    int bufferSize = 0;
    int inputChannels = 0;
    int outputChannels = 0;
    double inputLatencyMs = 0.0;
    double outputLatencyMs = 0.0;
    double roundTripLatencyMs = 0.0;
    bool isLowLatencyType = false;
    juce::String lastError;
};

/** What the caller wants changed. Empty/zero fields mean "leave as is". */
struct AudioDeviceRequest
{
    juce::String typeName;
    juce::String inputDeviceName;
    juce::String outputDeviceName;
    double sampleRate = 0.0;
    int bufferSize = 0;
};

/** One physical input channel of the open device. */
struct AudioInputPort
{
    juce::String name;

    /** Position in the callback's channel array, or -1 when the port is off. */
    int callbackPosition = -1;
};

/**
 * Owns every interaction with juce::AudioDeviceManager.
 *
 * The device manager is not thread safe, and the REST layer runs on arbitrary
 * Winsock connection threads, so every call here is marshalled onto the message
 * thread. Nothing outside this class may touch the device manager directly.
 */
class AudioDeviceController
{
public:
    explicit AudioDeviceController (juce::AudioDeviceManager& deviceManagerToUse);

    /** The buffer size and sample rate to aim for whenever a device is opened. */
    void setPreferred (double sampleRate, int bufferSize) noexcept;

    /**
     * Opens a device, restoring the saved state when it is usable and otherwise
     * walking the preferred device types from lowest latency downwards.
     * Safe to call from any thread; runs on the message thread.
     */
    juce::String initialise (const juce::XmlElement* savedState);

    /** Applies a partial change. Returns an empty string on success. */
    juce::String applyRequest (const AudioDeviceRequest& request);

    /**
     * Re-runs the low-latency search from scratch, ignoring whatever is open.
     *
     * Without this there is no way back: choosing a driver in the UI lands on
     * that driver's default buffer, and the saved state then pins it there on
     * every later launch.
     */
    juce::String optimiseForLowLatency();

    AudioDeviceSnapshot getSnapshot() const;

    /** Device types, their devices, and the rates/buffer sizes each supports. */
    juce::var describeAvailable() const;

    std::unique_ptr<juce::XmlElement> createStateXml() const;

    /**
     * Fired only when a caller explicitly asked for a rate or buffer size.
     *
     * Preferences must never be updated from whatever the device happened to
     * open at: doing that once made the app adopt a fallback 2048-sample buffer
     * as its target, so no later launch ever tried for anything lower.
     */
    std::function<void (double sampleRate, int bufferSize)> onUserRequestedSetup;

    // ---------------------------------------------------------- input ports
    //
    // Which physical jack feeds the engine's left channel and which feeds its
    // right. This is a description of the cables in the rig rather than of a
    // sound, so it lives in the settings file and never in a preset: a preset
    // that claimed "guitar on port 1" would be wrong the moment it was opened
    // on another interface, and wrong silently.

    /** Every input channel the open device has, with where each one lands. */
    std::vector<AudioInputPort> listInputPorts() const;

    /**
     * Chooses the ports by channel **name**, not by index.
     *
     * A name survives a device being reopened with a different channel count;
     * an index does not. Empty means "first available", which is what a fresh
     * install and every mono rig want.
     */
    void setInputPortNames (const juce::String& left, const juce::String& right);

    juce::String getInputPortName (bool right) const;

    /**
     * Recomputes where the chosen ports land in the callback's channel array.
     *
     * Must be called whenever the device changes, because JUCE hands the
     * callback one pointer per **active** channel in order -- not one per
     * physical port. With ports 2 and 4 active, `inputChannelData[0]` is port 2.
     * Resolving this once at startup would survive exactly until the first
     * device change, and the failure is silent: the wrong jack, or nothing.
     */
    void resolveInputPorts (juce::AudioIODevice* device);

    /**
     * Where to read the engine's left (or right) channel from, as an index into
     * the callback's array. -1 when that side has no source.
     *
     * Read on the audio thread; written on the message thread.
     */
    int getResolvedInput (bool right) const noexcept
    {
        return (right ? resolvedRight : resolvedLeft).load (std::memory_order_relaxed);
    }

    /** Fired on the message thread when the chosen ports changed. */
    std::function<void()> onInputPortsChanged;

    /**
     * Rank of the named channel among the active ones, or `fallback`.
     *
     * The whole port mapping turns on this, and getting it wrong is silent
     * rather than loud -- so it is a pure function, kept out of
     * `resolveInputPorts` purely so it can be tested without hardware.
     * An empty name means "no preference"; a name the device does not have
     * falls back rather than returning nothing, because silence is the worst
     * possible answer and the hardest to diagnose.
     */
    static int inputPositionFor (const juce::StringArray& channelNames,
                                 const juce::BigInteger& activeChannels,
                                 const juce::String& wanted,
                                 int fallback) noexcept;

    /** True when the open device type can realistically reach guitar-usable latency. */
    static bool isLowLatencyTypeName (const juce::String& typeName) noexcept;

    /** Device types in the order we would like to use them. */
    static juce::StringArray getPreferredTypeOrder();

private:
    juce::String initialiseOnMessageThread (const juce::XmlElement* savedState);

    /** Walks the preferred device types and opens the first that works. */
    juce::String openPreferredType();

    /** Steps the open device towards the preferred rate and buffer size. */
    void negotiateTowardsPreferred();

    /** True when the XML actually came from AudioDeviceManager::createStateXml. */
    static bool isUsableSavedState (const juce::XmlElement* savedState) noexcept;

    /** What "optimise" aims for; drivers clamp upwards from here on their own. */
    static constexpr int kLowestUsefulBufferSize = 32;

    /**
     * How many input channels to ask the manager for.
     *
     * This is a maximum, not a demand -- a two-in interface still opens with
     * two. It used to be 2, which made every jack past the second literally
     * unreachable: on a Scarlett 4i4 the two rear line inputs could not be
     * selected at all, because the device was never opened wide enough to see
     * them.
     */
    static constexpr int kMaxInputChannels = 8;

    std::vector<AudioInputPort> listInputPortsOnMessageThread() const;

    /** Bounds how long the search can take: each open is a hardware round trip. */
    static constexpr int kMaxDevicesPerType = 4;

    std::atomic<double> preferredSampleRate { 48000.0 };
    std::atomic<int> preferredBufferSize { 128 };

    /** Guards the two names; they are touched by REST and by device start. */
    juce::CriticalSection portLock;
    juce::String wantedInputLeft, wantedInputRight;

    std::atomic<int> resolvedLeft { 0 };
    std::atomic<int> resolvedRight { 1 };

    juce::String applyRequestOnMessageThread (const AudioDeviceRequest& request);
    AudioDeviceSnapshot snapshotOnMessageThread() const;
    juce::var describeAvailableOnMessageThread() const;

    /** Picks the smallest offered buffer that is not below the target. */
    static int chooseBufferSize (juce::AudioIODevice& device, int desired);

    juce::AudioDeviceManager& deviceManager;
};
} // namespace milodikfx::audio
