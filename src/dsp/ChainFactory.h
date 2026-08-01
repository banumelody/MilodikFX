#pragma once

#include <functional>
#include <vector>

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
#include "dsp/MixerProcessor.h"
#include "dsp/MetronomeProcessor.h"
#include "dsp/NamProcessor.h"
#include "dsp/NoiseGateProcessor.h"
#include "dsp/OverdriveProcessor.h"
#include "dsp/ReverbProcessor.h"
#include "dsp/SplitProcessor.h"
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

    /**
     * Where the signal becomes two paths, and where they become one again.
     *
     * Both are ordinary stages, so dragging them is how the parallel section is
     * positioned -- the same idea as Pedalboard's Router. Off by default: with
     * the split disabled the chain is serial and bit-identical to before.
     */
    SplitProcessor* split = nullptr;
    MixerProcessor* mixer = nullptr;

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

    /**
     * Instances 2..N of the types that can repeat; index 0 is instance 2.
     *
     * Fractal's model, not a dynamic registry: the inventory is fixed and every
     * instance is built at startup, which is what keeps `ParameterRegistry`
     * immutable and the VST3 parameter list a constant. Placing a block is
     * still just enabling one that already exists.
     *
     * Only duplicable types are set in an extra layer -- the amp head, the
     * split brackets and the master limiter stay null there, so the code that
     * registers a layer skips them without needing to know which is which.
     */
    std::vector<GuitarChain> extras;
};

/**
 * Which optional post-master stages a chain gets.
 *
 * Both default to on, so the app is unchanged. A plugin turns them off: neither
 * has a control surface there, and the looper is not free to carry around --
 * it allocates its whole 60-second record buffer at prepare time, which is
 * 23 MB at 48 kHz and 46 MB at 96 kHz *per instance*. A host also has its own
 * click and its own looping, so in a DAW they are dead weight twice over.
 */
struct ChainOptions
{
    bool withMetronome = true;
    bool withLooper = true;
};

/**
 * How many of each block the chain builds.
 *
 * Chosen from measurements rather than taste (`tests/InventoryTests.cpp` logs
 * them): at 96 kHz a delay costs ~1.5 MB, a cabinet ~1.2 MB and a reverb ~1 MB,
 * so this inventory adds about four megabytes over a single chain. The amp head
 * is the exception and it is a CPU limit, not a memory one -- one NAM Standard
 * model is already around 29 % of the budget at 96 kHz / 32 samples.
 */
struct ChainInventory
{
    int noiseGate = 2;
    int cleanBoost = 2;
    int compressor = 2;
    int overdrive = 3;
    int eq = 2;
    int toneStack = 2;
    int cabinet = 2;
    int delay = 2;
    int reverb = 2;

    /** The most instances any one type has, which is how many layers exist. */
    int maxInstances() const noexcept;
};

/**
 * Builds the signal chain in its fixed order and returns pointers into it.
 *
 * Shared by the standalone app and the plugin so the two can never disagree
 * about what the chain is or what order it runs in.
 */
GuitarChain buildGuitarChain (DSPChainManager& chain,
                              ChainOptions options = {},
                              ChainInventory inventory = {});

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
