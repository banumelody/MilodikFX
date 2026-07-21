#pragma once

#include <functional>

#include "api/ParameterRegistry.h"
#include "dsp/CabinetProcessor.h"
#include "dsp/CompressorProcessor.h"
#include "dsp/DSPChainManager.h"
#include "dsp/DelayProcessor.h"
#include "dsp/EQProcessor.h"
#include "dsp/GainProcessor.h"
#include "dsp/MasterOutProcessor.h"
#include "dsp/NoiseGateProcessor.h"
#include "dsp/OverdriveProcessor.h"
#include "dsp/ReverbProcessor.h"
#include "dsp/ToneStackProcessor.h"

namespace milodikfx::dsp
{
/** Non-owning pointers to the processors a built chain contains. */
struct GuitarChain
{
    NoiseGateProcessor* noiseGate = nullptr;
    GainProcessor* cleanBoost = nullptr;
    CompressorProcessor* compressor = nullptr;
    OverdriveProcessor* overdrive = nullptr;
    EQProcessor* eq = nullptr;
    ToneStackProcessor* toneStack = nullptr;
    CabinetProcessor* cabinet = nullptr;
    DelayProcessor* delay = nullptr;
    ReverbProcessor* reverb = nullptr;
    MasterOutProcessor* masterOut = nullptr;
};

/**
 * Builds the signal chain in its fixed order and returns pointers into it.
 *
 * Shared by the standalone app and the plugin so the two can never disagree
 * about what the chain is or what order it runs in.
 */
GuitarChain buildGuitarChain (DSPChainManager& chain);

/**
 * Registers every effect and parameter of a built chain.
 *
 * The input-routing stage is host-specific -- the standalone app maps device
 * channels itself, a plugin gets whatever the host sends -- so it is only added
 * when both accessors are supplied.
 */
void registerChainParameters (milodikfx::api::ParameterRegistry& registry,
                              const GuitarChain& chain,
                              std::function<float()> getInputMode = {},
                              std::function<void (float)> setInputMode = {});
} // namespace milodikfx::dsp
