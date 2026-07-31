#include "dsp/ChainOrder.h"

#include <algorithm>

namespace milodikfx::dsp
{
ChainOrder::ChainOrder (const GuitarChain& chain, DSPChainManager& managerToUse)
    : manager (managerToUse)
{
    // Build order, and it must match the order buildGuitarChain adds them in:
    // index N here is processor N in the manager. Adding a stage means adding it
    // in both places, which is why they sit next to each other in the file.
    const std::pair<const AudioProcessorBase*, const char*> stages[] = {
        { chain.inputTrim, "input" },
        { chain.noiseGate, "noiseGate" },
        { chain.cleanBoost, "cleanBoost" },
        { chain.compressor, "compressor" },
        { chain.split, "split" },
        { chain.overdrive, "overdrive" },
        { chain.eq, "eq" },
        { chain.toneStack, "toneStack" },
        { chain.nam, "nam" },
        { chain.cabinet, "cabinet" },
        { chain.delay, "delay" },
        { chain.reverb, "reverb" },
        { chain.mixer, "mixer" },
        { chain.masterOut, "master" },
    };

    for (const auto& [processor, id] : stages)
        if (processor != nullptr)
            stageIds.emplace_back (id);
}

bool ChainOrder::isFixed (const std::string& id) const noexcept
{
    // The two the engine pins, for reasons that are not taste: the input meter
    // reports `inputDb + trimDb` rather than measuring twice, and the master
    // stage carries the safety limiter.
    return id == "input" || id == "master";
}

std::vector<std::string> ChainOrder::getIds() const
{
    std::vector<std::string> ids;

    for (const auto index : manager.getOrder())
        if (index >= 0 && index < (int) stageIds.size())
            ids.push_back (stageIds[(size_t) index]);

    return ids;
}

bool ChainOrder::applyIds (const std::vector<std::string>& ids)
{
    std::vector<std::string> wanted;

    // Take the ids this build recognises, in the order asked for, ignoring
    // anything unknown and anything repeated.
    for (const auto& id : ids)
    {
        const auto known = std::find (stageIds.begin(), stageIds.end(), id) != stageIds.end();
        const auto alreadyTaken = std::find (wanted.begin(), wanted.end(), id) != wanted.end();

        if (known && ! alreadyTaken)
            wanted.push_back (id);
    }

    // Anything the list never mentioned goes back roughly where it was built,
    // rather than being dropped. A preset from an older build says nothing about
    // a stage that did not exist yet, and that must not delete it from the chain.
    for (size_t buildIndex = 0; buildIndex < stageIds.size(); ++buildIndex)
    {
        const auto& id = stageIds[buildIndex];

        if (std::find (wanted.begin(), wanted.end(), id) != wanted.end())
            continue;

        const auto insertAt = std::min (buildIndex, wanted.size());
        wanted.insert (wanted.begin() + (long) insertAt, id);
    }

    std::vector<int> indices;
    indices.reserve (wanted.size());

    for (const auto& id : wanted)
    {
        const auto found = std::find (stageIds.begin(), stageIds.end(), id);

        if (found == stageIds.end())
            return false;

        indices.push_back ((int) std::distance (stageIds.begin(), found));
    }

    // The manager has the final word: it refuses anything that moves a pinned
    // stage, so that rule is enforced in one place rather than trusted here.
    return manager.setOrder (indices);
}

std::vector<std::string> ChainOrder::getBusBIds() const
{
    std::vector<std::string> ids;

    for (size_t i = 0; i < stageIds.size(); ++i)
        if (manager.isStageOnBusB ((int) i))
            ids.push_back (stageIds[i]);

    return ids;
}

void ChainOrder::setBusBIds (const std::vector<std::string>& ids)
{
    for (size_t i = 0; i < stageIds.size(); ++i)
    {
        const auto wanted = std::find (ids.begin(), ids.end(), stageIds[i]) != ids.end();
        manager.setStageOnBusB ((int) i, wanted);
    }
}

void ChainOrder::reset()
{
    std::vector<int> identity;
    identity.reserve (stageIds.size());

    for (int i = 0; i < (int) stageIds.size(); ++i)
        identity.push_back (i);

    manager.setOrder (identity);
}
} // namespace milodikfx::dsp
