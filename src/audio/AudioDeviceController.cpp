#include "audio/AudioDeviceController.h"

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
 * The user's interface should win over the motherboard codec on first run.
 * Only consulted when there is no saved preference to restore.
 */
int indexOfPreferredDevice (const juce::StringArray& names)
{
    static const char* preferredSubstrings[] = { "Focusrite", "Scarlett", "USB Audio" };

    for (const auto* needle : preferredSubstrings)
        for (int i = 0; i < names.size(); ++i)
            if (names[i].containsIgnoreCase (needle))
                return i;

    return -1;
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

juce::String AudioDeviceController::initialise (const juce::XmlElement* savedState,
                                                int desiredBufferSize,
                                                double desiredSampleRate)
{
    // The saved state is owned by the caller and may die before the message
    // thread runs, so copy it into the lambda.
    std::shared_ptr<juce::XmlElement> stateCopy;

    if (savedState != nullptr)
        stateCopy = std::make_shared<juce::XmlElement> (*savedState);

    return runOnMessageThread ([this, stateCopy, desiredBufferSize, desiredSampleRate]
    {
        return initialiseOnMessageThread (stateCopy.get(), desiredBufferSize, desiredSampleRate);
    });
}

juce::String AudioDeviceController::initialiseOnMessageThread (const juce::XmlElement* savedState,
                                                               int desiredBufferSize,
                                                               double desiredSampleRate)
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

        juce::AudioDeviceManager::AudioDeviceSetup setup;

        const auto preferredOutput = indexOfPreferredDevice (outputNames);
        setup.outputDeviceName = outputNames[preferredOutput >= 0 ? preferredOutput : 0];

        if (! inputNames.isEmpty())
        {
            const auto preferredInput = indexOfPreferredDevice (inputNames);
            setup.inputDeviceName = inputNames[preferredInput >= 0 ? preferredInput : 0];
        }

        setup.useDefaultInputChannels = true;
        setup.useDefaultOutputChannels = true;

        // Exclusive mode in particular rejects a buffer size or rate it does not
        // offer, so relax the request one step at a time rather than writing the
        // whole device type off after a single "buffer size mismatch".
        const std::pair<double, int> attempts[] = {
            { desiredSampleRate, desiredBufferSize },
            { desiredSampleRate, 0 },
            { 0.0, desiredBufferSize },
            { 0.0, 0 }
        };

        juce::String error = "not attempted";

        for (const auto& [rate, buffer] : attempts)
        {
            setup.sampleRate = rate;
            setup.bufferSize = buffer;

            // initialise(), not setAudioDeviceSetup(): the manager only records
            // how many input channels are *needed* here. Going straight to
            // setAudioDeviceSetup leaves that at zero and opens output-only.
            error = deviceManager.initialise (2, 2, nullptr, false, setup.outputDeviceName, &setup);

            if (error.isEmpty() && deviceManager.getCurrentAudioDevice() != nullptr)
                break;
        }

        if (! error.isEmpty())
        {
            lastError = error;
            log ("Failed to open " + typeName + ": " + error);
            continue;
        }

        auto* device = deviceManager.getCurrentAudioDevice();

        if (device == nullptr)
            continue;

        // A processor with no input is useless, and some shared-mode drivers
        // happily open output-only. Treat that as a failure and keep looking.
        if (! inputNames.isEmpty() && device->getActiveInputChannels().countNumberOfSetBits() == 0)
        {
            lastError = typeName + " opened without any input channels";
            log ("Rejecting " + typeName + ": no input channels were opened");
            continue;
        }

        // The device is open, so it can now tell us what it really supports.
        // Walk it down towards the requested rate and buffer one step at a time,
        // rolling back anything it refuses -- a driver that rejects a setting
        // outright must not cost us the working device we already have.
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
            continue;

        if (device->getCurrentBufferSizeSamples() > desiredBufferSize)
        {
            auto candidates = device->getAvailableBufferSizes();
            candidates.sort();

            auto tried = 0;

            for (const auto candidate : candidates)
            {
                if (candidate < 32 || candidate >= device->getCurrentBufferSizeSamples())
                    continue;

                if (++tried > 4)
                    break;

                if (tryRefine (device->getCurrentSampleRate(), candidate))
                {
                    device = deviceManager.getCurrentAudioDevice();
                    break;
                }
            }
        }

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
        log ("Device change rejected: " + error);

    return error;
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
