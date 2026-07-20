#include "PresetsHandler.h"

#include "../dsp/GainProcessor.h"
#include "../dsp/OverdriveProcessor.h"
#include "../dsp/EQProcessor.h"
#include "../dsp/CompressorProcessor.h"
#include "../dsp/ReverbProcessor.h"
#include "../dsp/ToneStackProcessor.h"

HttpHandler::Response PresetsHandler::handleGet(
    const std::string& path,
    const std::string& query) const
{
    try
    {
        // Use PresetManager to list presets
        auto list = presetManager_.listPresets();
        juce::StringArray arr = list;

        std::string json = "{\n  \"presets\": [";
        for (int i = 0; i < arr.size(); ++i)
        {
            json += "\"" + arr[i].toStdString() + "\"";
            if (i < arr.size() - 1) json += ", ";
        }
        json += "]\n}";

        return { 200, "application/json", json };
    }
    catch (const std::exception& e)
    {
        return { 500, "application/json", std::string(R"({"error":"Exception: )") + e.what() + R"("})" };
    }
}

HttpHandler::Response PresetsHandler::handlePost(
    const std::string& path,
    const std::string& body)
{
    try
    {
        // Parse JSON body using JUCE JSON parser
        juce::var parsed;
        try {
            parsed = juce::JSON::parse(body);
        } catch (...) {
            return { 400, "application/json", R"({"error":"Invalid JSON body"})" };
        }

        if (!parsed.isObject())
            return { 400, "application/json", R"({"error":"Expected JSON object in body"})" };

        juce::var nameVar = parsed.getProperty("name", juce::var());
        if (nameVar.isVoid() || !nameVar.isString())
            return { 400, "application/json", R"({"error":"Missing or invalid 'name' field"})" };

        std::string presetName = nameVar.toString().toStdString();

        // Save preset
        if (path.find("/presets/save") != std::string::npos)
        {
            milodikfx::preset::PresetState state;
            auto& chain = engine_.getChain();

            if (auto* gp = chain.findProcessor<milodikfx::dsp::GainProcessor>())
            {
                state.cleanBoostEnabled = gp->isEnabled();
                state.cleanBoostGainDb = gp->getGainDb();
            }

            if (auto* od = chain.findProcessor<milodikfx::dsp::OverdriveProcessor>())
            {
                state.overdriveEnabled = od->isEnabled();
                state.overdriveDrivePct = od->getDrivePercent();
                state.overdriveLevelPct = od->getLevelPercent();
            }

            if (auto* eq = chain.findProcessor<milodikfx::dsp::EQProcessor>())
            {
                state.eqEnabled = eq->isEnabled();
                state.eqBassDb = eq->getBassDb();
                state.eqMidDb = eq->getMidDb();
                state.eqTrebleDb = eq->getTrebleDb();
            }

            if (auto* comp = chain.findProcessor<milodikfx::dsp::CompressorProcessor>())
            {
                state.compressorEnabled = comp->isEnabled();
                state.compressorInputGainDb = comp->getInputGainDb();
                state.compressorThresholdDb = comp->getThresholdDb();
                state.compressorRatio = comp->getRatio();
                state.compressorAttackMs = comp->getAttackMs();
                state.compressorReleaseMs = comp->getReleaseMs();
            }

            if (auto* re = chain.findProcessor<milodikfx::dsp::ReverbProcessor>())
            {
                state.reverbEnabled = re->isEnabled();
                state.reverbRoomSize = re->getRoomSize();
                state.reverbDryWetMix = re->getDryWetMix();
                state.reverbDecayTime = re->getDecayTime();
                state.reverbWidth = re->getWidth();
            }

            if (auto* ts = chain.findProcessor<milodikfx::dsp::ToneStackProcessor>())
            {
                state.toneStackEnabled = ts->isEnabled();
                state.toneStackBassDb = ts->getBassDb();
                state.toneStackMidDb = ts->getMidDb();
                state.toneStackTrebleDb = ts->getTrebleDb();
            }

            bool ok = presetManager_.savePreset (presetName, state);
            if (!ok)
                return { 500, "application/json", std::string("{\n  \"success\": false, \"message\": \"Failed to save preset\"\n}") };

            return { 200, "application/json", "{\n  \"success\": true, \"message\": \"Preset saved\"\n}" };
        }

        // Load preset
        if (path.find("/presets/load") != std::string::npos)
        {
            milodikfx::preset::PresetState state;
            bool ok = presetManager_.loadPreset(presetName, state);
            if (!ok)
                return { 404, "application/json", std::string("{\n  \"success\": false, \"message\": \"Preset not found\"\n}") };

            auto& chain = engine_.getChain();

            if (auto* gp = chain.findProcessor<milodikfx::dsp::GainProcessor>())
            {
                gp->setEnabled(state.cleanBoostEnabled);
                gp->setGainDb(state.cleanBoostGainDb);
            }

            if (auto* od = chain.findProcessor<milodikfx::dsp::OverdriveProcessor>())
            {
                od->setEnabled(state.overdriveEnabled);
                od->setDrivePercent(state.overdriveDrivePct);
                od->setLevelPercent(state.overdriveLevelPct);
            }

            if (auto* eq = chain.findProcessor<milodikfx::dsp::EQProcessor>())
            {
                eq->setEnabled(state.eqEnabled);
                eq->setBassDb(state.eqBassDb);
                eq->setMidDb(state.eqMidDb);
                eq->setTrebleDb(state.eqTrebleDb);
            }

            if (auto* comp = chain.findProcessor<milodikfx::dsp::CompressorProcessor>())
            {
                comp->setEnabled(state.compressorEnabled);
                comp->setInputGainDb(state.compressorInputGainDb);
                comp->setThresholdDb(state.compressorThresholdDb);
                comp->setRatio(state.compressorRatio);
                comp->setAttackMs(state.compressorAttackMs);
                comp->setReleaseMs(state.compressorReleaseMs);
            }

            if (auto* re = chain.findProcessor<milodikfx::dsp::ReverbProcessor>())
            {
                re->setEnabled(state.reverbEnabled);
                re->setRoomSize(state.reverbRoomSize);
                re->setDryWetMix(state.reverbDryWetMix);
                re->setDecayTime(state.reverbDecayTime);
                re->setWidth(state.reverbWidth);
            }

            if (auto* ts = chain.findProcessor<milodikfx::dsp::ToneStackProcessor>())
            {
                ts->setEnabled(state.toneStackEnabled);
                ts->setBassDb(state.toneStackBassDb);
                ts->setMidDb(state.toneStackMidDb);
                ts->setTrebleDb(state.toneStackTrebleDb);
            }

            return { 200, "application/json", "{\n  \"success\": true, \"message\": \"Preset loaded\"\n}" };
        }

        return { 400, "application/json", R"({"error":"Unknown preset action"})" };
    }
    catch (const std::exception& e)
    {
        return { 500, "application/json", std::string(R"({"error":"Exception: )") + e.what() + R"("})" };
    }
}
