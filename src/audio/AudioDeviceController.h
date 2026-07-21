#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <functional>
#include <memory>

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

    std::atomic<double> preferredSampleRate { 48000.0 };
    std::atomic<int> preferredBufferSize { 128 };
    juce::String applyRequestOnMessageThread (const AudioDeviceRequest& request);
    AudioDeviceSnapshot snapshotOnMessageThread() const;
    juce::var describeAvailableOnMessageThread() const;

    /** Picks the smallest offered buffer that is not below the target. */
    static int chooseBufferSize (juce::AudioIODevice& device, int desired);

    juce::AudioDeviceManager& deviceManager;
};
} // namespace milodikfx::audio
