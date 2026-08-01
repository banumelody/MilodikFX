#pragma once

#include <JuceHeader.h>

#include <atomic>

#include "api/HttpHandler.h"

/**
 * GET /api/levels - realtime metering.
 *
 * Written from the audio thread through plain relaxed atomic stores, read from
 * connection threads. Nothing here blocks or allocates on the audio side.
 */
class LevelsHandler final : public HttpHandler
{
public:
    LevelsHandler() = default;

    Response handleGet (const std::string& path, const std::string& query) const override;

    /**
     * @param inputDb       what the interface delivered, before the trim
     * @param chainInputDb  what the chain actually receives, after the trim
     *
     * Both, because they answer different questions. The trimmed figure is what
     * the Input knob is dialled against -- without it the knob would have no
     * feedback at all, since the meter is measured before the chain runs. The
     * untrimmed one still shows a signal arriving too hot from the interface,
     * which no amount of digital trim can fix.
     */
    void updateLevels (float inputDb, float chainInputDb, float outputDb) noexcept
    {
        updateLevels (inputDb, inputDb, chainInputDb, chainInputDb, outputDb, outputDb);
    }

    /**
     * Per channel, since the two sides can genuinely differ.
     *
     * They could not before v0.28: everything was mono and duplicated, so two
     * bars would have moved together forever. Pan per path, the stereo cabinet
     * mode and the L/R split changed that -- and the single figure is the
     * *louder* of the two, so the quieter side is currently invisible rather
     * than merely un-itemised.
     *
     * The combined fields stay, holding the maximum, so a client that never
     * asked for channels keeps working unchanged.
     */
    void updateLevels (float inputL, float inputR,
                       float chainInputL, float chainInputR,
                       float outputL, float outputR) noexcept
    {
        inputLevelL.store (inputL, std::memory_order_relaxed);
        inputLevelR.store (inputR, std::memory_order_relaxed);
        chainInputLevelL.store (chainInputL, std::memory_order_relaxed);
        chainInputLevelR.store (chainInputR, std::memory_order_relaxed);
        outputLevelL.store (outputL, std::memory_order_relaxed);
        outputLevelR.store (outputR, std::memory_order_relaxed);

        inputLevel.store (juce::jmax (inputL, inputR), std::memory_order_relaxed);
        chainInputLevel.store (juce::jmax (chainInputL, chainInputR), std::memory_order_relaxed);
        outputLevel.store (juce::jmax (outputL, outputR), std::memory_order_relaxed);
    }

    void updateGainReduction (float gateGainValue, float compressorDb, float limiterDb) noexcept
    {
        gateGain.store (gateGainValue, std::memory_order_relaxed);
        compressorReductionDb.store (compressorDb, std::memory_order_relaxed);
        limiterReductionDb.store (limiterDb, std::memory_order_relaxed);
    }

    void updateLoad (float cpuPercent, double sampleRate, int bufferSize) noexcept
    {
        cpuLoadPercent.store (cpuPercent, std::memory_order_relaxed);
        currentSampleRate.store (sampleRate, std::memory_order_relaxed);
        currentBufferSize.store (bufferSize, std::memory_order_relaxed);
    }

    void setAudioRunning (bool running) noexcept
    {
        audioRunning.store (running, std::memory_order_relaxed);
    }

    /**
     * Bumped whenever the chain changes in a way the UI did not initiate --
     * a footswitch recalling a scene, a MIDI CC moving a parameter, a program
     * change loading a preset. It rides along in the meter payload (already
     * streaming ~22 Hz), so the UI can notice and refetch without a new
     * connection or a new poll. Deliberately NOT bumped on ordinary parameter
     * writes: a UI knob drag would otherwise tell every client to refetch and
     * fight the drag it just made.
     */
    void bumpChainVersion() noexcept
    {
        chainVersion.fetch_add (1, std::memory_order_relaxed);
    }

    juce::uint32 getChainVersion() const noexcept
    {
        return chainVersion.load (std::memory_order_relaxed);
    }

    /**
     * Rides the meter payload so the looper panel does not need a poll of its
     * own. It used to ask /api/looper four times a second, which on a
     * thread-per-connection server is four sockets a second for a payload that
     * fits alongside one already going out ~22 times a second.
     *
     * Left absent until this is called, so a build with no looper -- the plugin --
     * simply reports no looper rather than a fabricated empty one.
     */
    void updateLooper (int state, bool loopPresent, float seconds, float positionFraction, float levelPercent) noexcept
    {
        looperState.store (state, std::memory_order_relaxed);
        looperHasLoop.store (loopPresent, std::memory_order_relaxed);
        looperSeconds.store (seconds, std::memory_order_relaxed);
        looperPosition.store (positionFraction, std::memory_order_relaxed);
        looperLevel.store (levelPercent, std::memory_order_relaxed);
        looperPresent.store (true, std::memory_order_relaxed);
    }

private:
    // Silence floor, reported until the audio callback delivers the first block.
    // Keep in sync with MainComponent::kMeterFloorDb.
    static constexpr float kFloorDb = -100.0f;

    std::atomic<float> inputLevel { kFloorDb };
    std::atomic<float> chainInputLevel { kFloorDb };
    std::atomic<float> outputLevel { kFloorDb };

    std::atomic<float> inputLevelL { kFloorDb };
    std::atomic<float> inputLevelR { kFloorDb };
    std::atomic<float> chainInputLevelL { kFloorDb };
    std::atomic<float> chainInputLevelR { kFloorDb };
    std::atomic<float> outputLevelL { kFloorDb };
    std::atomic<float> outputLevelR { kFloorDb };
    std::atomic<float> gateGain { 1.0f };
    std::atomic<float> compressorReductionDb { 0.0f };
    std::atomic<float> limiterReductionDb { 0.0f };
    std::atomic<float> cpuLoadPercent { 0.0f };
    std::atomic<double> currentSampleRate { 0.0 };
    std::atomic<int> currentBufferSize { 0 };
    std::atomic<bool> audioRunning { false };
    std::atomic<juce::uint32> chainVersion { 0 };

    std::atomic<bool> looperPresent { false };
    std::atomic<int> looperState { 0 };
    std::atomic<bool> looperHasLoop { false };
    std::atomic<float> looperSeconds { 0.0f };
    std::atomic<float> looperPosition { 0.0f };
    std::atomic<float> looperLevel { 100.0f };
};
