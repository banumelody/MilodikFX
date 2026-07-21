#include "api/ParameterRegistry.h"

namespace milodikfx::api
{
void ParameterRegistry::addEffect (EffectDescriptor effect)
{
    effects.push_back (std::move (effect));
}

namespace
{
// Ids are camelCase so they line up with the historical dsp.<effect>.<param>
// settings keys, but URLs should not have to care about case.
bool equalsIgnoreCase (const std::string& a, const std::string& b) noexcept
{
    if (a.size() != b.size())
        return false;

    for (size_t i = 0; i < a.size(); ++i)
        if (juce::CharacterFunctions::toLowerCase (a[i]) != juce::CharacterFunctions::toLowerCase (b[i]))
            return false;

    return true;
}
} // namespace

const EffectDescriptor* ParameterRegistry::findEffect (const std::string& effectId) const noexcept
{
    for (const auto& effect : effects)
        if (equalsIgnoreCase (effect.id, effectId))
            return &effect;

    return nullptr;
}

const ParameterDescriptor* ParameterRegistry::findParameter (const std::string& effectId,
                                                             const std::string& parameterId) const noexcept
{
    if (const auto* effect = findEffect (effectId))
        for (const auto& parameter : effect->parameters)
            if (equalsIgnoreCase (parameter.id, parameterId))
                return &parameter;

    return nullptr;
}

bool ParameterRegistry::setParameter (const std::string& effectId,
                                      const std::string& parameterId,
                                      float value,
                                      float& outApplied) const
{
    const auto* parameter = findParameter (effectId, parameterId);

    if (parameter == nullptr || ! parameter->set)
        return false;

    if (! std::isfinite (value))
        return false;

    const auto clamped = juce::jlimit (parameter->minValue, parameter->maxValue, value);
    parameter->set (clamped);

    // Read back rather than trusting the clamp: the processor may narrow it further.
    outApplied = parameter->get ? parameter->get() : clamped;

    notifyChanged();
    return true;
}

bool ParameterRegistry::setEffectEnabled (const std::string& effectId, bool enabled) const
{
    const auto* effect = findEffect (effectId);

    if (effect == nullptr || ! effect->setEnabled)
        return false;

    effect->setEnabled (enabled);
    notifyChanged();
    return true;
}

juce::var ParameterRegistry::effectToVar (const EffectDescriptor& effect) const
{
    auto* object = new juce::DynamicObject();

    object->setProperty ("id", juce::String (effect.id));
    object->setProperty ("label", juce::String (effect.label));
    object->setProperty ("description", juce::String (effect.description));
    object->setProperty ("enabled", effect.isEnabled ? effect.isEnabled() : true);

    juce::Array<juce::var> parameters;

    for (const auto& parameter : effect.parameters)
    {
        auto* p = new juce::DynamicObject();

        p->setProperty ("id", juce::String (parameter.id));
        p->setProperty ("label", juce::String (parameter.label));
        p->setProperty ("unit", juce::String (parameter.unit));
        p->setProperty ("min", parameter.minValue);
        p->setProperty ("max", parameter.maxValue);
        p->setProperty ("step", parameter.step);
        p->setProperty ("default", parameter.defaultValue);
        p->setProperty ("type", parameter.isBoolean ? "bool" : "float");
        p->setProperty ("value", parameter.get ? parameter.get() : parameter.defaultValue);

        parameters.add (juce::var (p));
    }

    object->setProperty ("parameters", parameters);

    return juce::var (object);
}

juce::var ParameterRegistry::toVar() const
{
    juce::Array<juce::var> array;

    for (const auto& effect : effects)
        array.add (effectToVar (effect));

    auto* root = new juce::DynamicObject();
    root->setProperty ("effects", array);

    return juce::var (root);
}

juce::var ParameterRegistry::captureState() const
{
    auto* root = new juce::DynamicObject();

    for (const auto& effect : effects)
    {
        auto* parameters = new juce::DynamicObject();

        for (const auto& parameter : effect.parameters)
            if (parameter.get)
                parameters->setProperty (juce::Identifier (parameter.id), parameter.get());

        auto* entry = new juce::DynamicObject();
        entry->setProperty ("enabled", effect.isEnabled ? effect.isEnabled() : true);
        entry->setProperty ("params", juce::var (parameters));

        root->setProperty (juce::Identifier (effect.id), juce::var (entry));
    }

    return juce::var (root);
}

int ParameterRegistry::applyState (const juce::var& state) const
{
    if (! state.isObject())
        return 0;

    auto applied = 0;

    for (const auto& effect : effects)
    {
        const auto entry = state[juce::Identifier (effect.id)];

        if (! entry.isObject())
            continue;

        const auto enabled = entry["enabled"];

        if (! enabled.isVoid() && effect.setEnabled)
        {
            effect.setEnabled ((bool) enabled);
            ++applied;
        }

        const auto parameters = entry["params"];

        if (! parameters.isObject())
            continue;

        for (const auto& parameter : effect.parameters)
        {
            const auto value = parameters[juce::Identifier (parameter.id)];

            if (value.isVoid() || value.isString() || ! parameter.set)
                continue;

            const auto asFloat = (float) (double) value;

            if (! std::isfinite (asFloat))
                continue;

            parameter.set (juce::jlimit (parameter.minValue, parameter.maxValue, asFloat));
            ++applied;
        }
    }

    if (applied > 0)
        notifyChanged();

    return applied;
}

void ParameterRegistry::forEachParameter (
    const std::function<void (const EffectDescriptor&, const ParameterDescriptor&)>& fn) const
{
    if (! fn)
        return;

    for (const auto& effect : effects)
        for (const auto& parameter : effect.parameters)
            fn (effect, parameter);
}

void ParameterRegistry::notifyChanged() const
{
    if (onChanged)
        onChanged();
}
} // namespace milodikfx::api
