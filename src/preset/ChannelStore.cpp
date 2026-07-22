#include "preset/ChannelStore.h"

namespace milodikfx::preset
{
namespace
{
const char* const kDefaultNames[] = { "A", "B", "C", "D" };
} // namespace

ChannelStore::ChannelStore (milodikfx::api::ParameterRegistry& registryToUse)
    : registry (registryToUse)
{
}

ChannelStore::Channel ChannelStore::captureEffect (const milodikfx::api::EffectDescriptor& effect) const
{
    Channel channel;
    channel.populated = true;

    for (const auto& parameter : effect.parameters)
    {
        if (parameter.isText)
        {
            if (parameter.getText)
                channel.textParams[parameter.id] = parameter.getText();
        }
        else if (parameter.get)
        {
            channel.params[parameter.id] = parameter.get();
        }
    }

    return channel;
}

void ChannelStore::applyChannel (const milodikfx::api::EffectDescriptor& effect, const Channel& channel) const
{
    for (const auto& parameter : effect.parameters)
    {
        if (parameter.isText)
        {
            const auto it = channel.textParams.find (parameter.id);

            if (it != channel.textParams.end() && parameter.setText)
                parameter.setText (it->second);

            continue;
        }

        const auto it = channel.params.find (parameter.id);

        if (it != channel.params.end() && parameter.set && std::isfinite (it->second))
            parameter.set (juce::jlimit (parameter.minValue, parameter.maxValue, it->second));
    }
}

ChannelStore::EffectChannels& ChannelStore::ensure (const milodikfx::api::EffectDescriptor& effect)
{
    auto it = byEffect.find (effect.id);

    if (it != byEffect.end())
        return it->second;

    // First time this effect is touched: seed all four channels from the sound
    // that is live right now, so every tab starts as a copy you then edit apart.
    EffectChannels created;
    const auto snapshot = captureEffect (effect);

    for (int i = 0; i < kNumChannels; ++i)
    {
        created.channels[(size_t) i] = snapshot;
        created.channels[(size_t) i].name = kDefaultNames[i];
    }

    created.active = 0;

    return byEffect.emplace (effect.id, std::move (created)).first->second;
}

bool ChannelStore::recall (const std::string& effectId, int index)
{
    if (! isValidIndex (index))
        return false;

    const auto* effect = registry.findEffect (effectId);

    if (effect == nullptr)
        return false;

    auto& entry = ensure (*effect);

    if (entry.active == index)
    {
        // Already here; nothing to jump to, but keep the channel in step with any
        // live edits so leaving and returning is lossless.
        entry.channels[(size_t) index] = captureEffect (*effect);
        entry.channels[(size_t) index].name = getName (effect->id, index);
        return true;
    }

    // Save the live values back into the outgoing channel first, so edits made
    // while on it are not lost, then apply the incoming one.
    if (isValidIndex (entry.active))
    {
        const auto keptName = entry.channels[(size_t) entry.active].name;
        entry.channels[(size_t) entry.active] = captureEffect (*effect);
        entry.channels[(size_t) entry.active].name = keptName;
    }

    applyChannel (*effect, entry.channels[(size_t) index]);
    entry.active = index;

    notifyChanged();
    return true;
}

bool ChannelStore::save (const std::string& effectId, int index)
{
    if (! isValidIndex (index))
        return false;

    const auto* effect = registry.findEffect (effectId);

    if (effect == nullptr)
        return false;

    auto& entry = ensure (*effect);
    const auto keptName = entry.channels[(size_t) index].name;

    entry.channels[(size_t) index] = captureEffect (*effect);
    entry.channels[(size_t) index].name = keptName;

    notifyChanged();
    return true;
}

bool ChannelStore::setName (const std::string& effectId, int index, const juce::String& name)
{
    if (! isValidIndex (index))
        return false;

    const auto trimmed = name.trim();

    if (trimmed.isEmpty())
        return false;

    const auto* effect = registry.findEffect (effectId);

    if (effect == nullptr)
        return false;

    ensure (*effect).channels[(size_t) index].name = trimmed.substring (0, 16);

    notifyChanged();
    return true;
}

int ChannelStore::getActive (const std::string& effectId) const
{
    const auto* effect = registry.findEffect (effectId);

    if (effect == nullptr)
        return 0;

    const auto it = byEffect.find (effect->id);

    return it != byEffect.end() ? it->second.active : 0;
}

juce::String ChannelStore::getName (const std::string& effectId, int index) const
{
    if (! isValidIndex (index))
        return {};

    const auto* effect = registry.findEffect (effectId);

    if (effect != nullptr)
        if (const auto it = byEffect.find (effect->id); it != byEffect.end())
            return it->second.channels[(size_t) index].name;

    return kDefaultNames[index];
}

juce::var ChannelStore::toVar() const
{
    auto* root = new juce::DynamicObject();

    for (const auto& [effectId, entry] : byEffect)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty ("active", entry.active);

        juce::Array<juce::var> slots;

        for (const auto& channel : entry.channels)
        {
            auto* slot = new juce::DynamicObject();
            slot->setProperty ("name", channel.name);
            slot->setProperty ("populated", channel.populated);

            auto* params = new juce::DynamicObject();
            for (const auto& [id, value] : channel.params)
                params->setProperty (juce::Identifier (id), value);
            slot->setProperty ("params", juce::var (params));

            auto* text = new juce::DynamicObject();
            for (const auto& [id, value] : channel.textParams)
                text->setProperty (juce::Identifier (id), value);
            slot->setProperty ("text", juce::var (text));

            slots.add (juce::var (slot));
        }

        object->setProperty ("slots", slots);
        root->setProperty (juce::Identifier (effectId), juce::var (object));
    }

    return juce::var (root);
}

void ChannelStore::fromVar (const juce::var& value)
{
    byEffect.clear();

    const auto* root = value.getDynamicObject();

    if (root == nullptr)
        return;

    for (const auto& effect : registry.getEffects())
    {
        const auto entryVar = value[juce::Identifier (effect.id)];

        if (! entryVar.isObject())
            continue;

        EffectChannels entry;
        entry.active = juce::jlimit (0, kNumChannels - 1, (int) entryVar["active"]);

        const auto* slots = entryVar["slots"].getArray();

        for (int i = 0; i < kNumChannels; ++i)
        {
            auto& channel = entry.channels[(size_t) i];
            channel.name = kDefaultNames[i];

            if (slots == nullptr || i >= slots->size())
                continue;

            const auto& slot = (*slots)[i];

            if (! slot.isObject())
                continue;

            if (const auto name = slot["name"].toString().trim(); name.isNotEmpty())
                channel.name = name.substring (0, 16);

            if (const auto* params = slot["params"].getDynamicObject())
                for (const auto& property : params->getProperties())
                    channel.params[property.name.toString().toStdString()] = (float) (double) property.value;

            if (const auto* text = slot["text"].getDynamicObject())
                for (const auto& property : text->getProperties())
                    channel.textParams[property.name.toString().toStdString()] = property.value.toString();

            channel.populated = (bool) slot["populated"] || ! channel.params.empty() || ! channel.textParams.empty();
        }

        byEffect[effect.id] = std::move (entry);
    }
}

void ChannelStore::resetToCurrent()
{
    byEffect.clear();

    for (const auto& effect : registry.getEffects())
        ensure (effect);
}

void ChannelStore::commitActive()
{
    for (auto& [effectId, entry] : byEffect)
    {
        const auto* effect = registry.findEffect (effectId);

        if (effect == nullptr || ! isValidIndex (entry.active))
            continue;

        const auto keptName = entry.channels[(size_t) entry.active].name;
        entry.channels[(size_t) entry.active] = captureEffect (*effect);
        entry.channels[(size_t) entry.active].name = keptName;
    }
}

void ChannelStore::notifyChanged() const
{
    if (onChanged)
        onChanged();
}
} // namespace milodikfx::preset
