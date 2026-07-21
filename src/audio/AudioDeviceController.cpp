#include "audio/AudioDeviceController.h"

#include <algorithm>
#include <numeric>
#include <vector>

namespace milodikfx::audio
{
namespace
{
constexpr int kMessageThreadTimeoutMs = 15000;

/**
 * Runs fn on the message thread and waits for the result.
 *
 * The shared state is captured by value, so if the wait times out the callback
 * can still complete safely instead of writing into a dead stack frame.
 */
template <typename Fn>
auto runOnMessageThread (Fn fn) -> decltype (fn())
{
    using Result = decltype (fn());

    auto* manager = juce::MessageManager::getInstanceWithoutCreating();

    if (manager == nullptr || manager->isThisTheMessageThread())
        return fn();

    struct Shared
    {
        juce::WaitableEvent done;
        Result result {};
    };

    auto shared = std::make_shared<Shared>();

    juce::MessageManager::callAsync ([shared, fn]() mutable
    {
        shared->result = fn();
        shared->done.signal();
    });

    if (! shared->done.wait (kMessageThreadTimeoutMs))
        return {};

    return shared->result;
}

void log (const juce::String& message)
{
    if (auto* logger = juce::Logger::getCurrentLogger())
        logger->writeToLog (message);
}

juce::String describeDeviceNames (juce::AudioIODeviceType& type, bool inputs)
{
    return type.getDeviceNames (inputs).joinIntoString (", ");
}

/**
 * Ranks a device name for a USB audio interface.
 *
 * Only decides the order things are tried in; anything that fails simply falls
 * through to the next candidate. The Thunderbolt penalty matters in practice:
 * Focusrite's driver package registers a Thunderbolt ASIO driver even on a
 * machine with no Thunderbolt hardware, and it always fails to open.
 */
int scoreDeviceName (const juce::String& name)
{
    auto score = 0;

    if (name.containsIgnoreCase ("Scarlett"))     score += 100;
    if (name.containsIgnoreCase ("Focusrite"))    score += 40;
    if (name.containsIgnoreCase ("USB"))          score += 30;
    if (name.containsIgnoreCase ("Thunderbolt"))  score -= 80;
    if (name.containsIgnoreCase ("Primary Sound")) score -= 40;

    return score;
}

/** Device indices, best candidate first. */
std::vector<int> rankedDeviceIndices (const juce::StringArray& names)
{
    std::vector<int> order ((size_t) names.size());
    std::iota (order.begin(), order.end(), 0);

    std::stable_sort (order.begin(), order.end(), [&names] (int a, int b)
    {
        return scoreDeviceName (names[a]) > scoreDeviceName (names[b]);
    });

    return order;
}

/**
 * Picks the input to pair with a chosen output. ASIO exposes one driver as both
 * sides, so an exact name match wins; otherwise take the best-ranked input.
 */
juce::String chooseInputFor (const juce::StringArray& inputNames, const juce::String& outputName)
{
    if (inputNames.isEmpty())
        return {};

    for (const auto& name : inputNames)
        if (name == outputName)
            return name;

    const auto ranked = rankedDeviceIndices (inputNames);
    return inputNames[ranked.front()];
}
} // namespace

AudioDeviceController::AudioDeviceController (juce::AudioDeviceManager& deviceManagerToUse)
    : deviceManager (deviceManagerToUse)
{
}

juce::StringArray AudioDeviceController::getPreferredTypeOrder()
{
    // ASIO first (lowest latency, needs the Steinberg SDK at build time), then
    // WASAPI exclusive, then WASAPI's IAudioClient3 low-latency shared mode.
    // Plain shared mode and DirectSound are the fallbacks that always work.
    return { "ASIO",
             "Windows Audio (Exclusive Mode)",
             "Windows Audio (Low Latency Mode)",
             "Windows Audio",
             "DirectSound" };
}

bool AudioDeviceController::isUsableSavedState (const juce::XmlElement* savedState) noexcept
{
    // AudioDeviceManager silently ignores an element it does not recognise and
    // then opens the default device, reporting no error -- which used to look
    // exactly like a successful restore and skipped the low-latency search.
    return savedState != nullptr && savedState->hasTagName ("DEVICESETUP");
}

bool AudioDeviceController::isLowLatencyTypeName (const juce::String& typeName) noexcept
{
    return typeName.equalsIgnoreCase ("ASIO")
           || typeName.containsIgnoreCase ("Exclusive")
           || typeName.containsIgnoreCase ("Low Latency");
}

int AudioDeviceController::chooseBufferSize (juce::AudioIODevice& device, int desired)
{
    const auto available = device.getAvailableBufferSizes();

    if (available.isEmpty())
        return desired;

    auto best = available[available.size() - 1];

    for (const auto size : available)
        if (size >= desired && size < best)
            best = size;

    // Everything on offer is smaller than we asked for, so take the largest.
    if (best < desired)
        for (const auto size : available)
            best = juce::jmax (best, size);

    return best;
}

void AudioDeviceController::setPreferred (double sampleRate, int bufferSize) noexcept
{
    if (sampleRate > 0.0)
        preferredSampleRate.store (sampleRate, std::memory_order_relaxed);

    if (bufferSize > 0)
        preferredBufferSize.store (bufferSize, std::memory_order_relaxed);
}

juce::String AudioDeviceController::initialise (const juce::XmlElement* savedState)
{
    // The saved state is owned by the caller and may die before the message
    // thread runs, so copy it into the lambda.
    std::shared_ptr<juce::XmlElement> stateCopy;

    if (savedState != nullptr)
        stateCopy = std::make_shared<juce::XmlElement> (*savedState);

    return runOnMessageThread ([this, stateCopy]
    {
        return initialiseOnMessageThread (stateCopy.get());
    });
}

juce::String AudioDeviceController::optimiseForLowLatency()
{
    const auto error = runOnMessageThread ([this]
    {
        log ("Re-running the low-latency device search on request");

        // Aim at the floor, not at the stored preference. Once someone has
        // picked, say, 480 samples by hand, that becomes the preference -- and a
        // button labelled "optimise latency" that targets the value you are
        // trying to get away from is useless.
        const auto previousPreference = preferredBufferSize.exchange (kLowestUsefulBufferSize,
                                                                      std::memory_order_relaxed);

        const auto result = openPreferredType();

        if (! result.isEmpty())
            preferredBufferSize.store (previousPreference, std::memory_order_relaxed);

        return result;
    });

    if (error.isNotEmpty())
        return error;

    // Whatever the search settled on becomes the new target, so the next launch
    // restores it instead of drifting back to the old preference.
    const auto snapshot = getSnapshot();

    if (snapshot.bufferSize > 0)
    {
        setPreferred (snapshot.sampleRate, snapshot.bufferSize);

        if (onUserRequestedSetup)
            onUserRequestedSetup (snapshot.sampleRate, snapshot.bufferSize);
    }

    return {};
}

juce::String AudioDeviceController::initialiseOnMessageThread (const juce::XmlElement* savedState)
{
    auto types = juce::StringArray();

    for (auto* type : deviceManager.getAvailableDeviceTypes())
        types.add (type->getTypeName());

    log ("Available audio device types: " + types.joinIntoString (", "));

    // First choice: exactly what was open last time.
    if (isUsableSavedState (savedState))
    {
        const auto error = deviceManager.initialise (2, 2, savedState, true);

        if (error.isEmpty() && deviceManager.getCurrentAudioDevice() != nullptr)
        {
            log ("Restored saved audio device state ("
                 + deviceManager.getCurrentAudioDeviceType() + ")");
            return {};
        }

        log ("Saved audio device state could not be restored (" + error + "); falling back");
    }
    else if (savedState != nullptr)
    {
        log ("Ignoring saved audio state in an unrecognised format; searching for a low-latency device");
    }

    return openPreferredType();
}

juce::String AudioDeviceController::openPreferredType()
{
    const auto desiredSampleRate = preferredSampleRate.load (std::memory_order_relaxed);
    const auto desiredBufferSize = preferredBufferSize.load (std::memory_order_relaxed);

    juce::StringArray types;

    for (auto* type : deviceManager.getAvailableDeviceTypes())
        types.add (type->getTypeName());

    juce::String lastError;

    for (const auto& typeName : getPreferredTypeOrder())
    {
        if (! types.contains (typeName))
            continue;

        deviceManager.setCurrentAudioDeviceType (typeName, true);

        auto* type = deviceManager.getCurrentDeviceTypeObject();

        if (type == nullptr)
            continue;

        type->scanForDevices();

        const auto outputNames = type->getDeviceNames (false);
        const auto inputNames = type->getDeviceNames (true);

        if (outputNames.isEmpty())
        {
            log ("Device type " + typeName + " offers no outputs");
            continue;
        }

        log ("Trying " + typeName + " -- outputs: " + describeDeviceNames (*type, false)
             + " | inputs: " + describeDeviceNames (*type, true));

        // Try several devices within the type, not just the best-ranked one.
        // ASIO in particular exposes every installed driver as a device, and the
        // first pick can be one with no hardware behind it -- abandoning the
        // whole type at that point skipped the driver we actually wanted.
        auto attemptsLeft = kMaxDevicesPerType;

        for (const auto outputIndex : rankedDeviceIndices (outputNames))
        {
            if (attemptsLeft-- <= 0)
                break;

            juce::AudioDeviceManager::AudioDeviceSetup setup;
            setup.outputDeviceName = outputNames[outputIndex];
            setup.inputDeviceName = chooseInputFor (inputNames, setup.outputDeviceName);
            setup.useDefaultInputChannels = true;
            setup.useDefaultOutputChannels = true;

            // Only the sample rate is negotiated here; the buffer is left to the
            // driver and tuned afterwards, once the open device can say what it
            // actually supports. Guessing at a buffer up front cost a failed
            // open per guess, each a second or more of hardware round trip.
            const double rateAttempts[] = { desiredSampleRate, 0.0 };

            juce::String error = "not attempted";

            for (const auto rate : rateAttempts)
            {
                setup.sampleRate = rate;
                setup.bufferSize = 0;

                // initialise(), not setAudioDeviceSetup(): the manager only
                // records how many input channels are *needed* here. Going
                // straight to setAudioDeviceSetup leaves that at zero and opens
                // the device output-only.
                error = deviceManager.initialise (2, 2, nullptr, false, setup.outputDeviceName, &setup);

                if (error.isEmpty() && deviceManager.getCurrentAudioDevice() != nullptr)
                    break;
            }

            if (! error.isEmpty())
            {
                lastError = error;
                log ("  " + setup.outputDeviceName + " failed: " + error);
                continue;
            }

            auto* device = deviceManager.getCurrentAudioDevice();

            if (device == nullptr)
                continue;

            // A processor with no input is useless, and some shared-mode drivers
            // happily open output-only. Treat that as a failure and keep looking.
            if (! inputNames.isEmpty() && device->getActiveInputChannels().countNumberOfSetBits() == 0)
            {
                lastError = setup.outputDeviceName + " opened without any input channels";
                log ("  " + setup.outputDeviceName + " rejected: no input channels");
                continue;
            }

            negotiateTowardsPreferred();

            device = deviceManager.getCurrentAudioDevice();

            if (device == nullptr)
                continue;

            const auto rate = device->getCurrentSampleRate();
            const auto block = device->getCurrentBufferSizeSamples();

            log ("Opened " + typeName + " / " + device->getName()
                 + " @ " + juce::String (rate) + " Hz"
                 + ", buffer " + juce::String (block)
                 + " (" + juce::String (rate > 0.0 ? 1000.0 * block / rate : 0.0, 2) + " ms)"
                 + ", inputs " + juce::String (device->getActiveInputChannels().countNumberOfSetBits()));

            return {};
        }
    }

    // Nothing preferred worked, so let JUCE pick whatever it can.
    const auto fallbackError = deviceManager.initialiseWithDefaultDevices (2, 2);

    if (! fallbackError.isEmpty())
    {
        log ("All audio device types failed: " + fallbackError);
        return fallbackError;
    }

    if (! lastError.isEmpty())
        log ("Fell back to the default device after: " + lastError);

    return {};
}

void AudioDeviceController::negotiateTowardsPreferred()
{
    auto* device = deviceManager.getCurrentAudioDevice();

    if (device == nullptr)
        return;

    const auto desiredSampleRate = preferredSampleRate.load (std::memory_order_relaxed);
    const auto desiredBufferSize = preferredBufferSize.load (std::memory_order_relaxed);

    // The device is open, so it can now tell us what it really supports. Step it
    // towards the target and roll back anything it refuses -- a driver that
    // rejects a setting outright must not cost us the working device we have.
    auto tryRefine = [this] (double rate, int buffer)
    {
        auto current = deviceManager.getAudioDeviceSetup();
        const auto previous = current;

        current.sampleRate = rate;
        current.bufferSize = buffer;

        const auto refineError = deviceManager.setAudioDeviceSetup (current, true);
        auto* refined = deviceManager.getCurrentAudioDevice();

        if (refineError.isEmpty() && refined != nullptr
            && refined->getActiveInputChannels().countNumberOfSetBits() > 0)
            return true;

        log ("  refine to " + juce::String (rate) + " Hz / " + juce::String (buffer)
             + " rejected: " + (refineError.isEmpty() ? juce::String ("no input channels") : refineError));

        deviceManager.setAudioDeviceSetup (previous, true);
        return false;
    };

    if (std::abs (device->getCurrentSampleRate() - desiredSampleRate) > 1.0)
    {
        const auto rates = device->getAvailableSampleRates();

        if (rates.contains (desiredSampleRate)
            && tryRefine (desiredSampleRate, device->getCurrentBufferSizeSamples()))
            device = deviceManager.getCurrentAudioDevice();
    }

    if (device == nullptr)
        return;

    if (device->getCurrentBufferSizeSamples() > desiredBufferSize)
    {
        auto candidates = device->getAvailableBufferSizes();
        candidates.sort();

        // Each attempt closes and reopens the hardware, so cap them: three
        // tries is enough to find the floor on every driver seen so far, and
        // more than that makes the UI feel hung.
        auto tried = 0;

        for (const auto candidate : candidates)
        {
            if (candidate < 32
                || candidate < desiredBufferSize
                || candidate >= device->getCurrentBufferSizeSamples())
                continue;

            if (++tried > 3)
                break;

            if (tryRefine (device->getCurrentSampleRate(), candidate))
                break;
        }
    }
}

juce::String AudioDeviceController::applyRequest (const AudioDeviceRequest& request)
{
    const auto error = runOnMessageThread ([this, request]
    {
        return applyRequestOnMessageThread (request);
    });

    if (error.isEmpty() && onUserRequestedSetup
        && (request.sampleRate > 0.0 || request.bufferSize > 0))
        onUserRequestedSetup (request.sampleRate, request.bufferSize);

    return error;
}

juce::String AudioDeviceController::applyRequestOnMessageThread (const AudioDeviceRequest& request)
{
    if (request.typeName.isNotEmpty()
        && ! request.typeName.equalsIgnoreCase (deviceManager.getCurrentAudioDeviceType()))
    {
        juce::StringArray types;

        for (auto* type : deviceManager.getAvailableDeviceTypes())
            types.add (type->getTypeName());

        if (! types.contains (request.typeName))
            return "Unknown device type: " + request.typeName;

        deviceManager.setCurrentAudioDeviceType (request.typeName, true);

        if (auto* type = deviceManager.getCurrentDeviceTypeObject())
            type->scanForDevices();
    }

    auto setup = deviceManager.getAudioDeviceSetup();

    if (request.inputDeviceName.isNotEmpty())
        setup.inputDeviceName = request.inputDeviceName;

    if (request.outputDeviceName.isNotEmpty())
        setup.outputDeviceName = request.outputDeviceName;

    if (request.sampleRate > 0.0)
        setup.sampleRate = request.sampleRate;

    if (request.bufferSize > 0)
        setup.bufferSize = request.bufferSize;

    setup.useDefaultInputChannels = true;
    setup.useDefaultOutputChannels = true;

    const auto error = deviceManager.setAudioDeviceSetup (setup, true);

    if (error.isNotEmpty())
    {
        log ("Device change rejected: " + error);
        return error;
    }

    // Changing driver or device otherwise leaves us on that driver's default
    // buffer, which is typically ten times larger than we want. Only skip this
    // when the caller named a buffer size explicitly -- that is their choice.
    if (request.bufferSize <= 0)
        negotiateTowardsPreferred();

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        const auto rate = device->getCurrentSampleRate();
        const auto block = device->getCurrentBufferSizeSamples();

        log ("Device now: " + deviceManager.getCurrentAudioDeviceType() + " / " + device->getName()
             + " @ " + juce::String (rate) + " Hz, buffer " + juce::String (block)
             + " (" + juce::String (rate > 0.0 ? 1000.0 * block / rate : 0.0, 2) + " ms)");
    }

    return {};
}

AudioDeviceSnapshot AudioDeviceController::getSnapshot() const
{
    return runOnMessageThread ([this] { return snapshotOnMessageThread(); });
}

AudioDeviceSnapshot AudioDeviceController::snapshotOnMessageThread() const
{
    AudioDeviceSnapshot snapshot;

    snapshot.typeName = deviceManager.getCurrentAudioDeviceType();
    snapshot.isLowLatencyType = isLowLatencyTypeName (snapshot.typeName);

    // Read the live device rather than getAudioDeviceSetup(), which comes back
    // empty even while a device is running.
    auto* device = deviceManager.getCurrentAudioDevice();

    if (device == nullptr)
        return snapshot;

    snapshot.isOpen = device->isOpen();
    snapshot.inputDeviceName = device->getName();
    snapshot.outputDeviceName = device->getName();
    snapshot.sampleRate = device->getCurrentSampleRate();
    snapshot.bufferSize = device->getCurrentBufferSizeSamples();
    snapshot.inputChannels = device->getActiveInputChannels().countNumberOfSetBits();
    snapshot.outputChannels = device->getActiveOutputChannels().countNumberOfSetBits();

    const auto setup = deviceManager.getAudioDeviceSetup();

    if (setup.inputDeviceName.isNotEmpty())
        snapshot.inputDeviceName = setup.inputDeviceName;

    if (setup.outputDeviceName.isNotEmpty())
        snapshot.outputDeviceName = setup.outputDeviceName;

    if (snapshot.sampleRate > 0.0)
    {
        snapshot.inputLatencyMs = 1000.0 * device->getInputLatencyInSamples() / snapshot.sampleRate;
        snapshot.outputLatencyMs = 1000.0 * device->getOutputLatencyInSamples() / snapshot.sampleRate;
        snapshot.roundTripLatencyMs = snapshot.inputLatencyMs + snapshot.outputLatencyMs;
    }

    return snapshot;
}

juce::var AudioDeviceController::describeAvailable() const
{
    return runOnMessageThread ([this] { return describeAvailableOnMessageThread(); });
}

juce::var AudioDeviceController::describeAvailableOnMessageThread() const
{
    juce::Array<juce::var> typeArray;

    for (auto* type : deviceManager.getAvailableDeviceTypes())
    {
        if (type == nullptr)
            continue;

        auto* typeObject = new juce::DynamicObject();
        typeObject->setProperty ("name", type->getTypeName());
        typeObject->setProperty ("lowLatency", isLowLatencyTypeName (type->getTypeName()));

        juce::Array<juce::var> inputs;
        for (const auto& name : type->getDeviceNames (true))
            inputs.add (name);

        juce::Array<juce::var> outputs;
        for (const auto& name : type->getDeviceNames (false))
            outputs.add (name);

        typeObject->setProperty ("inputs", inputs);
        typeObject->setProperty ("outputs", outputs);

        typeArray.add (juce::var (typeObject));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("types", typeArray);
    root->setProperty ("currentType", deviceManager.getCurrentAudioDeviceType());

    juce::Array<juce::var> sampleRates;
    juce::Array<juce::var> bufferSizes;

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        for (const auto rate : device->getAvailableSampleRates())
            sampleRates.add (rate);

        for (const auto size : device->getAvailableBufferSizes())
            bufferSizes.add (size);
    }

    root->setProperty ("availableSampleRates", sampleRates);
    root->setProperty ("availableBufferSizes", bufferSizes);

    return juce::var (root);
}

std::unique_ptr<juce::XmlElement> AudioDeviceController::createStateXml() const
{
    // createStateXml returns a unique_ptr, which the generic helper cannot move
    // through its shared result slot, so serialise to text and reparse instead.
    const auto text = runOnMessageThread ([this]() -> juce::String
    {
        if (auto xml = deviceManager.createStateXml())
            return xml->toString();

        return {};
    });

    if (text.isEmpty())
        return {};

    return juce::parseXML (text);
}
} // namespace milodikfx::audio
