#pragma once

#include <string>
#include <vector>

#include "dsp/ChainFactory.h"
#include "dsp/DSPChainManager.h"

namespace milodikfx::dsp
{
/**
 * Translates the chain's processing order between effect ids and processor
 * indices.
 *
 * Everything outside the engine speaks **ids** -- the REST API, presets, the
 * settings file, the plugin's state blob. Ids are stable across versions in a
 * way indices are not: a preset written when the chain had eleven stages still
 * means something in a build that has twelve, whereas every index in it would
 * have quietly shifted by one.
 *
 * The manager itself only ever sees indices, because on the audio thread an
 * index is a nibble and a string is a disaster.
 */
class ChainOrder final
{
public:
    /** Binds to a built chain. The stage list is fixed from here on. */
    ChainOrder (const GuitarChain& chain, DSPChainManager& manager);

    /** The order currently in force, as effect ids. */
    std::vector<std::string> getIds() const;

    /**
     * Reorders the chain from a list of effect ids.
     *
     * Forgiving in both directions, because a preset is a document that outlives
     * the build that wrote it: ids this build does not have are ignored, and
     * stages the list never mentions are put back at their original position
     * rather than being dropped. Returns false only when the result would move a
     * pinned stage -- the input trim off the front, or anything after the master
     * limiter.
     */
    bool applyIds (const std::vector<std::string>& ids);

    /** Restores the order the chain was built in. */
    void reset();

    /** Every stage, in build order. Index N here is processor N. */
    const std::vector<std::string>& getStageIds() const noexcept { return stageIds; }

    /** True for a stage that may never be moved. */
    bool isFixed (const std::string& id) const noexcept;

    /**
     * Which stages run on path B of the parallel section, as effect ids.
     *
     * Only meaningful between the split and the mixer; a stage outside that
     * range keeps its flag but the engine ignores it, so dragging a stage out of
     * the section and back does not lose which side it was on.
     */
    std::vector<std::string> getBusBIds() const;
    void setBusBIds (const std::vector<std::string>& ids);

private:
    std::vector<std::string> stageIds;
    DSPChainManager& manager;
};
} // namespace milodikfx::dsp
