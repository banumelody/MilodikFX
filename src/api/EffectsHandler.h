#pragma once

#include <JuceHeader.h>

#include "api/HttpHandler.h"
#include "api/ParameterRegistry.h"
#include "preset/ChannelStore.h"

/**
 * /api/effects/*
 *
 *   GET  /api/effects                       every effect, in chain order
 *   GET  /api/effects/{effect}              one effect with its parameters
 *   POST /api/effects/{effect}/enabled      { "enabled": bool }
 *   PUT  /api/effects/{effect}/{param}      { "value": x }
 *   PUT  /api/effects/{effect}/channel      { "value": 0..3 }   select A/B/C/D
 *   POST /api/effects/{effect}/channel/save { "value": 0..3 }   store into a channel
 *
 * When a channel store is attached, every effect in the GET responses also
 * carries `channel` (the active index) and `channels` (the four names).
 */
class EffectsHandler final : public HttpHandler
{
public:
    explicit EffectsHandler (const milodikfx::api::ParameterRegistry& registry)
        : registry_ (registry)
    {
    }

    /** Optional: a plugin build has no channel store. */
    void setChannelStore (milodikfx::preset::ChannelStore* store) { channelStore_ = store; }

    Response handleGet (const std::string& path, const std::string& query) const override;
    Response handlePost (const std::string& path, const std::string& body) override;
    Response handlePut (const std::string& path, const std::string& body) override;

private:
    /** Adds `channel`/`channels` to an effect var, in place, when a store exists. */
    void augment (juce::var& effectVar) const;
    juce::var effectWithChannels (const milodikfx::api::EffectDescriptor& effect) const;

    const milodikfx::api::ParameterRegistry& registry_;
    milodikfx::preset::ChannelStore* channelStore_ = nullptr;
};
