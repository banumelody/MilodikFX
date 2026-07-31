#pragma once

#include <functional>

#include "api/ApiJson.h"
#include "api/HttpHandler.h"
#include "dsp/ChainOrder.h"

/**
 * /api/chain
 *
 *   GET /api/chain/order   { order, fixed, busB, placed }
 *   PUT /api/chain/order   { order: [ids] }   -> the order actually applied
 *   PUT /api/chain/buses   { busB: [ids] }    -> which stages run on path B
 *   PUT /api/chain/board   { placed: [ids] }  -> which stages are on the board
 *
 * The order is a list of effect ids rather than positions, because an id
 * survives a version change and a position does not. Unknown ids are ignored and
 * unmentioned stages keep their place, so a preset written by an older build
 * still loads into something sensible.
 */
class ChainOrderHandler final : public HttpHandler
{
public:
    explicit ChainOrderHandler (milodikfx::dsp::ChainOrder& orderToUse) : order (orderToUse) {}

    /** Marked dirty so a reorder survives a restart and reaches the preset file. */
    std::function<void()> onChanged;

    Response handleGet (const std::string& path, const std::string&) const override
    {
        using namespace milodikfx::api;

        const auto segments = pathSegmentsAfter (path, "/api/chain");

        if (segments.size() != 1)
            return jsonError (404, "Expected /api/chain/order, /buses or /board");

        const auto action = toLowerAscii (segments[0]);

        if (action != "order" && action != "buses" && action != "board")
            return jsonError (404, "Expected /api/chain/order, /buses or /board");

        return jsonOk (stateVar());
    }

    Response handlePut (const std::string& path, const std::string& body) override
    {
        using namespace milodikfx::api;

        const auto segments = pathSegmentsAfter (path, "/api/chain");

        if (segments.size() != 1)
            return jsonError (404, "Expected /api/chain/order, /buses or /board");

        const auto action = toLowerAscii (segments[0]);

        if (action == "buses")
        {
            const auto parsedBuses = parseBody (body);
            const auto* onB = parsedBuses["busB"].getArray();

            if (onB == nullptr)
                return jsonError (400, "Body must contain a 'busB' array of effect ids");

            std::vector<std::string> ids;

            for (const auto& item : *onB)
                ids.push_back (item.toString().toStdString());

            order.setBusBIds (ids);

            if (onChanged)
                onChanged();

            return jsonOk (stateVar());
        }

        if (action == "board")
        {
            const auto parsedBoard = parseBody (body);
            const auto* onBoard = parsedBoard["placed"].getArray();

            if (onBoard == nullptr)
                return jsonError (400, "Body must contain a 'placed' array of effect ids");

            std::vector<std::string> ids;

            for (const auto& item : *onBoard)
                ids.push_back (item.toString().toStdString());

            // The complete set, not a delta: anything left out comes off the
            // board. Fixed stages go back on regardless -- see ChainOrder.
            order.setPlacedIds (ids);

            if (onChanged)
                onChanged();

            return jsonOk (stateVar());
        }

        if (action != "order")
            return jsonError (404, "Expected /api/chain/order, /buses or /board");

        const auto parsed = parseBody (body);
        const auto* list = parsed["order"].getArray();

        if (list == nullptr)
            return jsonError (400, "Body must contain an 'order' array of effect ids");

        std::vector<std::string> ids;

        for (const auto& item : *list)
            ids.push_back (item.toString().toStdString());

        // The only way this fails is a pinned stage being moved: the input trim
        // off the front, or something after the master limiter.
        if (! order.applyIds (ids))
            return jsonError (409, "That order moves a stage that is fixed in place");

        if (onChanged)
            onChanged();

        return jsonOk (stateVar());
    }

private:
    juce::var stateVar() const
    {
        juce::Array<juce::var> current;

        for (const auto& id : order.getIds())
            current.add (juce::String (id));

        juce::Array<juce::var> fixed;

        for (const auto& id : order.getStageIds())
            if (order.isFixed (id))
                fixed.add (juce::String (id));

        juce::Array<juce::var> busB;

        for (const auto& id : order.getBusBIds())
            busB.add (juce::String (id));

        juce::Array<juce::var> placed;

        for (const auto& id : order.getPlacedIds())
            placed.add (juce::String (id));

        auto* root = new juce::DynamicObject();
        root->setProperty ("order", current);
        root->setProperty ("fixed", fixed);
        root->setProperty ("busB", busB);
        root->setProperty ("placed", placed);

        return juce::var (root);
    }

    milodikfx::dsp::ChainOrder& order;
};
