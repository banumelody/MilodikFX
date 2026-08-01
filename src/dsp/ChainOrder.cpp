#include "dsp/ChainOrder.h"

#include <algorithm>
#include <string>

namespace milodikfx::dsp
{
ChainOrder::ChainOrder (const GuitarChain& chain, DSPChainManager& managerToUse)
    : manager (managerToUse)
{
    // Build order, and it must match the order buildGuitarChain adds them in:
    // index N here is processor N in the manager. Adding a stage means adding it
    // in both places, which is why they sit next to each other in the file.
    // Instance 1 keeps the bare id it has always had; later instances get a
    // numeric suffix. That is what lets every preset, settings file and MIDI
    // mapping written before duplicate blocks existed keep working with no
    // migration at all -- the ids they name are still exactly these.
    const auto layerFor = [&chain] (size_t instance) -> const GuitarChain&
    {
        return instance == 0 ? chain : chain.extras[instance - 1];
    };

    const auto suffixFor = [] (size_t instance)
    {
        return instance == 0 ? std::string() : std::to_string (instance + 1);
    };

    const auto addInstances = [&] (auto GuitarChain::* member, const char* id)
    {
        for (size_t instance = 0; instance <= chain.extras.size(); ++instance)
            if (layerFor (instance).*member != nullptr)
                stageIds.emplace_back (std::string (id) + suffixFor (instance));
    };

    // Must match the order buildGuitarChain adds them in: index N here is
    // processor N in the manager. They sit next to each other in the file for
    // exactly that reason.
    if (chain.inputTrim != nullptr) stageIds.emplace_back ("input");
    addInstances (&GuitarChain::noiseGate, "noiseGate");
    addInstances (&GuitarChain::cleanBoost, "cleanBoost");
    addInstances (&GuitarChain::compressor, "compressor");
    if (chain.split != nullptr) stageIds.emplace_back ("split");
    addInstances (&GuitarChain::overdrive, "overdrive");
    addInstances (&GuitarChain::eq, "eq");
    addInstances (&GuitarChain::toneStack, "toneStack");
    if (chain.nam != nullptr) stageIds.emplace_back ("nam");
    addInstances (&GuitarChain::cabinet, "cabinet");
    addInstances (&GuitarChain::delay, "delay");
    addInstances (&GuitarChain::reverb, "reverb");
    if (chain.mixer != nullptr) stageIds.emplace_back ("mixer");
    if (chain.masterOut != nullptr) stageIds.emplace_back ("master");
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

std::vector<std::string> ChainOrder::getPlacedIds() const
{
    std::vector<std::string> ids;

    for (size_t i = 0; i < stageIds.size(); ++i)
        if (manager.isStagePlaced ((int) i))
            ids.push_back (stageIds[i]);

    return ids;
}

void ChainOrder::setPlacedIds (const std::vector<std::string>& ids)
{
    for (size_t i = 0; i < stageIds.size(); ++i)
    {
        // Fixed stages go back on whatever the list said. The manager refuses to
        // remove them anyway; asking for it here would just make the two layers
        // disagree about what the board contains.
        const auto wanted = isFixed (stageIds[i])
                            || std::find (ids.begin(), ids.end(), stageIds[i]) != ids.end();

        manager.setStagePlaced ((int) i, wanted);
    }
}

void ChainOrder::placeAll()
{
    for (size_t i = 0; i < stageIds.size(); ++i)
        manager.setStagePlaced ((int) i, true);
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
