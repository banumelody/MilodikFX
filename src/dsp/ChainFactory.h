#pragma once

#include <functional>

#include "api/ParameterRegistry.h"
#include "dsp/CabinetProcessor.h"
#include "dsp/CompressorProcessor.h"
#include "dsp/DSPChainManager.h"
#include "dsp/DelayProcessor.h"
#include "dsp/EQProcessor.h"
#include "dsp/GainProcessor.h"
#include "dsp/InputTrimProcessor.h"
#include "dsp/LooperProcessor.h"
#include "dsp/MasterOutProcessor.h"
#include "dsp/MetronomeProcessor.h"
#include "dsp/NamProcessor.h"
#include "dsp/NoiseGateProcessor.h"
#include "dsp/OverdriveProcessor.h"
#include "dsp/ReverbProcessor.h"
#include "dsp/ToneStackProcessor.h"
#include "preset/IrLibrary.h"
#include "preset/NamLibrary.h"

namespace milodikfx::dsp
{
/** Non-owning pointers to the processors a built chain contains. */
struct GuitarChain
{
    /** Ahead of the gate, so the gate threshold tracks the trim. */
    InputTrimProcessor* inputTrim = nullptr;

    NoiseGateProcessor* noiseGate = nullptr;
    GainProcessor* cleanBoost = nullptr;
    CompressorProcessor* compressor = nullptr;
    OverdriveProcessor* overdrive = nullptr;
    EQProcessor* eq = nullptr;
    ToneStackProcessor* toneStack = nullptr;

    /** The amp head, between the tone shaping and the cabinet. */
    NamProcessor* nam = nullptr;

    CabinetProcessor* cabinet = nullptr;
    DelayProcessor* delay = nullptr;
    ReverbProcessor* reverb = nullptr;
    MasterOutProcessor* masterOut = nullptr;

    /** Sits after the master stage, outside the bypass path. */
    MetronomeProcessor* metronome = nullptr;

    /** Also after the master stage: the loop plays on across a global bypass. */
    LooperProcessor* looper = nullptr;
};

/**
 * Builds the signal chain in its fixed order and returns pointers into it.
 *
 * Shared by the standalone app and the plugin so the two can never disagree
 * about what the chain is or what order it runs in.
 */
GuitarChain buildGuitarChain (DSPChainManager& chain);

/**
 * Host-provided pieces the chain can use but does not own.
 *
 * Everything here is optional: a plugin has no device to route and may have no
 * impulse-response library, and the corresponding controls are simply left out
 * rather than registered in a broken state.
 */
struct ChainExtras
{
    /** Supplies the impulse responses the cabinet and reverb can load. */
    milodikfx::preset::IrLibrary* irLibrary = nullptr;

    /** Supplies the NAM amp-head models the head stage can load. */
    milodikfx::preset::NamLibrary* namLibrary = nullptr;

    /** Both must be set for the input-routing stage to be registered. */
    std::function<float()> getInputMode;
    std::function<void (float)> setInputMode;
};

/**
 * Registers every effect and parameter of a built chain.
 *
 * The input-routing stage is host-specific -- the standalone app maps device
 * channels itself, a plugin gets whatever the host sends -- so it is only added
 * when both accessors are supplied.
 */
void registerChainParameters (milodikfx::api::ParameterRegistry& registry,
                              const GuitarChain& chain,
                              DSPChainManager& manager,
                              ChainExtras extras = {});
} // namespace milodikfx::dsp
