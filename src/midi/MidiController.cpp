#include "midi/MidiController.h"

#include <cmath>

namespace milodikfx::midi
{
namespace
{
constexpr int kMessageThreadTimeoutMs = 5000;

/** Above this a footswitch counts as pressed; the MIDI convention. */
constexpr int kSwitchThreshold = 64;

/**
 * Runs fn on the message thread and waits for the result.
 *
 * Same shape as the helper in AudioDeviceController, and for the same reason:
 * shared state is captured by value, so a timed-out wait cannot leave the
 * callback writing into a dead stack frame.
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

/**
 * Hands work to the message thread, or runs it here when there is none.
 *
 * The fallback is what lets a headless unit test observe these callbacks; in
 * the app a message manager always exists, so anything touching files or the
 * UI still leaves the MIDI thread.
 */
template <typename Fn>
void postToMessageThread (Fn&& fn)
{
    auto* manager = juce::MessageManager::getInstanceWithoutCreating();

    if (manager == nullptr || manager->isThisTheMessageThread())
    {
        fn();
        return;
    }

    juce::MessageManager::callAsync (std::forward<Fn> (fn));
}
} // namespace

MidiController::MidiController (milodikfx::api::ParameterRegistry& registryToUse)
    : registry (registryToUse)
{
}

MidiController::~MidiController()
{
    if (input != nullptr)
    {
        input->stop();
        input.reset();
    }
}

juce::StringArray MidiController::listDevices()
{
    juce::StringArray names;

    for (const auto& device : juce::MidiInput::getAvailableDevices())
        names.add (device.name);

    return names;
}

juce::String MidiController::openDevice (const juce::String& deviceName)
{
    return runOnMessageThread ([this, deviceName]() -> juce::String
    {
        if (input != nullptr)
        {
            input->stop();
            input.reset();
            openName.clear();
        }

        if (deviceName.isEmpty())
        {
            if (onConfigurationChanged)
                onConfigurationChanged();

            return {};
        }

        for (const auto& device : juce::MidiInput::getAvailableDevices())
        {
            if (device.name != deviceName)
                continue;

            auto opened = juce::MidiInput::openDevice (device.identifier, this);

            if (opened == nullptr)
                return "Tidak bisa membuka perangkat MIDI: " + deviceName;

            input = std::move (opened);
            input->start();
            openName = deviceName;

            if (onConfigurationChanged)
                onConfigurationChanged();

            return {};
        }

        return "Perangkat MIDI tidak ditemukan: " + deviceName;
    });
}

juce::String MidiController::getOpenDeviceName() const
{
    return openName;
}

bool MidiController::isOpen() const
{
    return input != nullptr;
}

void MidiController::setMapping (int ccNumber, const Mapping& mapping)
{
    if (ccNumber < 0 || ccNumber >= kNumControllers)
        return;

    {
        const juce::ScopedLock scoped (lock);
        mappings[(size_t) ccNumber] = mapping;
    }

    if (onConfigurationChanged)
        onConfigurationChanged();
}

void MidiController::clearMapping (int ccNumber)
{
    setMapping (ccNumber, {});
}

Mapping MidiController::getMapping (int ccNumber) const
{
    if (ccNumber < 0 || ccNumber >= kNumControllers)
        return {};

    const juce::ScopedLock scoped (lock);
    return mappings[(size_t) ccNumber];
}

std::vector<std::pair<int, Mapping>> MidiController::getMappings() const
{
    std::vector<std::pair<int, Mapping>> result;

    const juce::ScopedLock scoped (lock);

    for (int cc = 0; cc < kNumControllers; ++cc)
        if (mappings[(size_t) cc].isValid())
            result.emplace_back (cc, mappings[(size_t) cc]);

    return result;
}

void MidiController::startLearning (const Mapping& target)
{
    const juce::ScopedLock scoped (lock);
    learnTarget = target;
}

void MidiController::stopLearning()
{
    startLearning ({});
}

bool MidiController::isLearning() const
{
    const juce::ScopedLock scoped (lock);
    return learnTarget.isValid();
}

Mapping MidiController::getLearnTarget() const
{
    const juce::ScopedLock scoped (lock);
    return learnTarget;
}

void MidiController::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    if (message.isProgramChange())
    {
        const auto program = message.getProgramChangeNumber();

        // Selecting a preset reads a file, so it cannot happen here.
        if (onProgramChange)
            postToMessageThread ([callback = onProgramChange, program] { callback (program); });

        return;
    }

    if (! message.isController())
        return;

    const auto cc = message.getControllerNumber();
    const auto value = message.getControllerValue();

    lastCc.store (cc, std::memory_order_relaxed);
    lastCcValue.store (value, std::memory_order_relaxed);

    Mapping target;
    auto learned = false;

    {
        const juce::ScopedLock scoped (lock);

        if (learnTarget.isValid())
        {
            // Bind on the first controller that moves, then disarm. Binding on
            // release as well would immediately overwrite the mapping with the
            // same one and fire the callback twice.
            mappings[(size_t) cc] = learnTarget;
            target = learnTarget;
            learnTarget = {};
            learned = true;
        }
        else
        {
            target = mappings[(size_t) cc];
        }
    }

    if (learned)
    {
        if (onLearned)
            postToMessageThread ([callback = onLearned, cc, target] { callback (cc, target); });

        // Do not also act on the message that did the binding: stepping on a
        // switch to assign it should not toggle the thing it was just assigned.
        return;
    }

    if (! target.isValid())
        return;

    applyControlChange (cc, value);
}

void MidiController::applyControlChange (int ccNumber, int value)
{
    const auto target = getMapping (ccNumber);

    if (! target.isValid())
        return;

    // Scene and channel recalls fire on the press, like a footswitch, and touch
    // the scene/channel stores -- which are not built for the MIDI thread, so
    // they go to the message thread the same way a program change does.
    if (target.kind == MappingKind::scene)
    {
        if (value >= kSwitchThreshold && onSceneRecall)
            postToMessageThread ([callback = onSceneRecall, index = target.index] { callback (index); });

        return;
    }

    if (target.kind == MappingKind::channel)
    {
        if (value >= kSwitchThreshold && onChannelSelect)
            postToMessageThread ([callback = onChannelSelect, effectId = target.effectId, index = target.index]
                                 { callback (effectId, index); });

        return;
    }

    const auto effectId = target.effectId.toStdString();
    const auto parameterId = target.parameterId.toStdString();

    // "enabled" is not a parameter but the effect's own switch, and is by far
    // the most useful thing to put under a footswitch.
    const auto isEnableSwitch = target.parameterId.equalsIgnoreCase ("enabled");

    if (isEnableSwitch)
    {
        const auto* effect = registry.findEffect (effectId);

        if (effect == nullptr || ! effect->setEnabled)
            return;

        if (target.mode == MappingMode::toggle)
        {
            if (value < kSwitchThreshold)
                return; // release; the press already toggled

            registry.setEffectEnabled (effectId, ! effect->isEnabled());
        }
        else
        {
            registry.setEffectEnabled (effectId, value >= kSwitchThreshold);
        }
    }
    else
    {
        const auto* parameter = registry.findParameter (effectId, parameterId);

        if (parameter == nullptr || parameter->isText || ! parameter->set)
            return;

        float applied = 0.0f;

        if (target.mode == MappingMode::toggle)
        {
            if (value < kSwitchThreshold)
                return;

            const auto current = parameter->get ? parameter->get() : parameter->minValue;
            const auto midpoint = (parameter->minValue + parameter->maxValue) * 0.5f;
            const auto next = current >= midpoint ? parameter->minValue : parameter->maxValue;

            registry.setParameter (effectId, parameterId, next, applied);
        }
        else
        {
            const auto position = (float) value / 127.0f;
            const auto scaled = parameter->minValue
                                + position * (parameter->maxValue - parameter->minValue);

            registry.setParameter (effectId, parameterId, scaled, applied);
        }
    }

    if (onParameterChanged)
        postToMessageThread ([callback = onParameterChanged] { callback(); });
}
} // namespace milodikfx::midi
