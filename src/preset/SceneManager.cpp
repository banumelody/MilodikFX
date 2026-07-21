#include "preset/SceneManager.h"

namespace milodikfx::preset
{
namespace
{
/** Names a guitarist would reach for, in the order the footswitches sit. */
const char* const kDefaultNames[] = { "Clean", "Crunch", "Lead", "Solo" };
} // namespace

SceneManager::SceneManager (milodikfx::api::ParameterRegistry& registryToUse)
    : registry (registryToUse)
{
    for (int i = 0; i < kNumScenes; ++i)
        scenes[(size_t) i].name = kDefaultNames[i];
}

const SceneManager::Scene& SceneManager::getScene (int index) const
{
    static const Scene empty;

    return isValidIndex (index) ? scenes[(size_t) index] : empty;
}

bool SceneManager::capture (int index)
{
    if (! isValidIndex (index))
        return false;

    auto& scene = scenes[(size_t) index];
    scene.enabled.clear();

    for (const auto& effect : registry.getEffects())
    {
        // Stages that are always in the path have no flag worth storing, and
        // recalling one would be a no-op at best.
        if (! effect.setEnabled || ! effect.isEnabled)
            continue;

        scene.enabled[effect.id] = effect.isEnabled();
    }

    scene.populated = true;
    activeIndex = index;

    return true;
}

bool SceneManager::recall (int index)
{
    if (! isValidIndex (index))
        return false;

    const auto& scene = scenes[(size_t) index];

    if (! scene.populated)
        return false;

    for (const auto& [effectId, enabled] : scene.enabled)
        registry.setEffectEnabled (effectId, enabled);

    activeIndex = index;

    return true;
}

bool SceneManager::setEffectEnabled (int index, const juce::String& effectId, bool enabled)
{
    if (! isValidIndex (index) || effectId.isEmpty())
        return false;

    const auto id = effectId.toStdString();
    const auto* effect = registry.findEffect (id);

    if (effect == nullptr || ! effect->setEnabled)
        return false;

    auto& scene = scenes[(size_t) index];

    // Editing a slot that was never captured would otherwise store one flag and
    // leave the rest unknown, so fill it from the chain first.
    if (! scene.populated)
    {
        const auto previousActive = activeIndex;
        capture (index);
        activeIndex = previousActive;
    }

    // Stored under the registry's own id, not the caller's spelling, so a
    // case-insensitive URL cannot create a second entry for the same effect.
    scene.enabled[effect->id] = enabled;

    // The chain no longer matches whatever was last recalled.
    if (activeIndex == index)
        activeIndex = -1;

    return true;
}

bool SceneManager::setName (int index, const juce::String& name)
{
    if (! isValidIndex (index))
        return false;

    const auto trimmed = name.trim();

    if (trimmed.isEmpty())
        return false;

    scenes[(size_t) index].name = trimmed.substring (0, 24);

    return true;
}

juce::var SceneManager::toVar() const
{
    juce::Array<juce::var> array;

    for (const auto& scene : scenes)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty ("name", scene.name);
        object->setProperty ("populated", scene.populated);

        auto* flags = new juce::DynamicObject();

        for (const auto& [effectId, enabled] : scene.enabled)
            flags->setProperty (juce::Identifier (effectId), enabled);

        object->setProperty ("enabled", juce::var (flags));

        array.add (juce::var (object));
    }

    return array;
}

void SceneManager::fromVar (const juce::var& value)
{
    const auto* array = value.getArray();

    if (array == nullptr)
        return;

    for (int i = 0; i < kNumScenes; ++i)
    {
        auto& scene = scenes[(size_t) i];

        scene.name = kDefaultNames[i];
        scene.enabled.clear();
        scene.populated = false;

        if (i >= array->size())
            continue;

        const auto& entry = (*array)[i];

        if (! entry.isObject())
            continue;

        if (const auto name = entry["name"].toString().trim(); name.isNotEmpty())
            scene.name = name.substring (0, 24);

        const auto flags = entry["enabled"];

        if (const auto* object = flags.getDynamicObject())
        {
            for (const auto& property : object->getProperties())
                scene.enabled[property.name.toString().toStdString()] = (bool) property.value;
        }

        scene.populated = (bool) entry["populated"] && ! scene.enabled.empty();
    }

    // The chain has not been made to match any of them yet.
    activeIndex = -1;
}

void SceneManager::resetToCurrent()
{
    for (int i = 0; i < kNumScenes; ++i)
    {
        scenes[(size_t) i].name = kDefaultNames[i];
        capture (i);
    }

    activeIndex = -1;
}
} // namespace milodikfx::preset
