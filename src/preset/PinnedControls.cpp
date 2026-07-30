#include "preset/PinnedControls.h"

namespace milodikfx::preset
{
namespace
{
const juce::Identifier kEffectProperty { "effect" };
const juce::Identifier kParameterProperty { "parameter" };
} // namespace

bool PinnedControls::isPinned (const std::string& effectId, const std::string& parameterId) const noexcept
{
    for (int i = 0; i < count; ++i)
        if (pins[(size_t) i].effectId == effectId && pins[(size_t) i].parameterId == parameterId)
            return true;

    return false;
}

bool PinnedControls::add (const std::string& effectId, const std::string& parameterId)
{
    if (effectId.empty() || parameterId.empty())
        return false;

    if (count >= kMaxPins || isPinned (effectId, parameterId))
        return false;

    pins[(size_t) count] = { effectId, parameterId };
    ++count;
    return true;
}

bool PinnedControls::remove (const std::string& effectId, const std::string& parameterId)
{
    for (int i = 0; i < count; ++i)
    {
        if (pins[(size_t) i].effectId != effectId || pins[(size_t) i].parameterId != parameterId)
            continue;

        // Close the gap: the order is the order they appear on the stage screen,
        // so leaving a hole would shuffle the rest of them under the player's
        // fingers the next time one is removed.
        for (int j = i; j + 1 < count; ++j)
            pins[(size_t) j] = pins[(size_t) (j + 1)];

        --count;
        pins[(size_t) count] = {};
        return true;
    }

    return false;
}

bool PinnedControls::toggle (const std::string& effectId, const std::string& parameterId)
{
    if (remove (effectId, parameterId))
        return false;

    return add (effectId, parameterId);
}

juce::var PinnedControls::toVar() const
{
    juce::Array<juce::var> array;

    for (int i = 0; i < count; ++i)
    {
        auto* entry = new juce::DynamicObject();
        entry->setProperty (kEffectProperty, juce::String (pins[(size_t) i].effectId));
        entry->setProperty (kParameterProperty, juce::String (pins[(size_t) i].parameterId));
        array.add (juce::var (entry));
    }

    return array;
}

void PinnedControls::fromVar (const juce::var& value, const milodikfx::api::ParameterRegistry* registry)
{
    clear();

    const auto* array = value.getArray();

    if (array == nullptr)
        return;

    for (const auto& item : *array)
    {
        const auto effectId = item[kEffectProperty].toString().toStdString();
        const auto parameterId = item[kParameterProperty].toString().toStdString();

        if (effectId.empty() || parameterId.empty())
            continue;

        // A pin that names something this build does not have would put a knob
        // on the stage screen with nothing behind it.
        if (registry != nullptr && registry->findParameter (effectId, parameterId) == nullptr)
            continue;

        if (! add (effectId, parameterId))
            break; // full
    }
}
} // namespace milodikfx::preset
