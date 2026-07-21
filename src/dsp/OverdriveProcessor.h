#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <memory>

#include "dsp/AudioProcessorBase.h"
#include "dsp/Biquad.h"
#include "dsp/DriveVoicing.h"

namespace milodikfx::dsp
{
/**
 * Cubic soft-clipper with selectable oversampling and adjustable asymmetry.
 *
 * The clipper is a hard nonlinearity at high drive, which folds harmonics back
 * below Nyquist at 48 kHz; oversampling pushes most of that fizz out of the
 * audible band. Asymmetry biases the waveform before clipping so the curve is
 * no longer odd-symmetric, which is what produces the even harmonics a valve
 * stage is known for.
 */
class OverdriveProcessor final : public AudioProcessorBase
{
public:
    OverdriveProcessor();
    ~OverdriveProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels) override;
    void processBlock (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setDrivePercent (float percent) noexcept;
    float getDrivePercent() const noexcept;

    void setLevelPercent (float percent) noexcept;
    float getLevelPercent() const noexcept;

    void setEnabled (bool shouldEnable) noexcept;
    bool isEnabled() const noexcept;

    void setOversamplingEnabled (bool shouldEnable) noexcept;
    bool isOversamplingEnabled() const noexcept;

    /** 0 = off, 1 = 2x, 2 = 4x, 3 = 8x. Switching never allocates. */
    void setOversamplingIndex (int index) noexcept;
    int getOversamplingIndex() const noexcept;

    /** 0 = symmetric (the classic curve), 1 = maximum valve-like bias. */
    void setAsymmetry (float amount) noexcept;
    float getAsymmetry() const noexcept;

    /**
     * Which pedal this is voiced as. 0 keeps the original untouched behaviour,
     * so presets written before voicings existed sound exactly as they did.
     */
    void setType (int type) noexcept;
    int getType() const noexcept;

    /** Post-clip tone sweep, 0..100. Ignored by voicings with no Tone knob. */
    void setTonePercent (float percent) noexcept;
    float getTonePercent() const noexcept;

    /** Shifts where the clipper's low cut sits. Dumble's Voice control. */
    void setVoicePercent (float percent) noexcept;
    float getVoicePercent() const noexcept;

    void setBassDb (float db) noexcept;
    float getBassDb() const noexcept;

    void setMidDb (float db) noexcept;
    float getMidDb() const noexcept;

    void setTrebleDb (float db) noexcept;
    float getTrebleDb() const noexcept;

    /** OCD's HP mode: more low end into the clipper and more of it. */
    void setHighPeakMode (bool shouldUseHp) noexcept;
    bool isHighPeakMode() const noexcept;

    /** The EP booster's treble lift. */
    void setBright (bool shouldBrighten) noexcept;
    bool isBright() const noexcept;

    /** Extra latency introduced by the active oversampler, in samples. */
    float getLatencySamples() const noexcept;

private:
    // One oversampler per factor, all built up front: changing factor mid-stream
    // would otherwise mean allocating on the audio thread.
    static constexpr int kNumOversamplers = 3; // 2x, 4x, 8x
    static constexpr int kMaxBlockChannels = 32;

    /** Largest bias offset applied before clipping at asymmetry = 1. */
    static constexpr float kMaxBias = 0.3f;

    static float softClip (float x) noexcept;

    void applyDrive (float* const* channels,
                     int numChannels,
                     int numSamples,
                     float driveTarget,
                     float asymmetryTarget) noexcept;

    void applyLevel (float* const* channels, int numChannels, int numSamples, float levelTarget) noexcept;

    /** The voiced path: split, clip, shape. Used for every type but Custom. */
    void applyVoicedDrive (float* const* channels,
                           int numChannels,
                           int numSamples,
                           double rate,
                           float driveTarget) noexcept;

    /** Rebuilds the voicing filters when the type, a knob or the rate moved. */
    void updateVoicingIfNeeded (double rate) noexcept;

    static constexpr int kMaxVoiceChannels = 8;

    /** Filter state for one channel of the voiced path. */
    struct VoiceState
    {
        BiquadState split;
        BiquadState splitHigh;
        BiquadState tone;
        BiquadState presence;
        BiquadState bass;
        BiquadState mid;
        BiquadState treble;
        DcBlocker dc;

        void reset() noexcept
        {
            split.reset();
            splitHigh.reset();
            tone.reset();
            presence.reset();
            bass.reset();
            mid.reset();
            treble.reset();
            dc.reset();
        }
    };

    std::array<VoiceState, kMaxVoiceChannels> voiceStates {};

    /** The oversampler for the current index, or nullptr when oversampling is off. */
    juce::dsp::Oversampling<float>* activeOversampler() const noexcept;

    std::atomic<float> drivePercent { 0.0f };   // 0..100
    std::atomic<float> driveAmount { 0.0f };    // 0..1

    std::atomic<float> levelPercent { 100.0f }; // 0..100
    std::atomic<float> levelLinear { 1.0f };    // 0..1

    std::atomic<float> asymmetry { 0.0f };      // 0..1

    std::atomic<int> type { drive::custom };
    std::atomic<float> tonePercent { 50.0f };
    std::atomic<float> voicePercent { 50.0f };
    std::atomic<float> bassDb { 0.0f };
    std::atomic<float> midDb { 0.0f };
    std::atomic<float> trebleDb { 0.0f };
    std::atomic<bool> highPeakMode { false };
    std::atomic<bool> bright { false };

    // Audio-thread owned. Rebuilt only when one of the inputs actually moved.
    BiquadCoeffs splitCoeffs, splitHighCoeffs, toneCoeffs, presenceCoeffs;
    BiquadCoeffs bassCoeffs, midCoeffs, trebleCoeffs;
    int builtForType = -1;
    float builtForTone = -1.0f;
    float builtForVoice = -1.0f;
    float builtForBass = -99.0f;
    float builtForMid = -99.0f;
    float builtForTreble = -99.0f;
    bool builtForHp = false;
    bool builtForBright = false;
    double builtForRate = 0.0;

    std::atomic<bool> enabled { true };
    std::atomic<bool> oversamplingEnabled { true };
    std::atomic<int> oversamplingIndex { 1 };   // default 2x, matching the original behaviour
    bool prepared = false;

    int preparedChannels = 0;
    int preparedBlockSize = 0;
    double preparedRate = 44100.0;

    SmoothedParam smoothedDrive;
    SmoothedParam smoothedLevel;
    SmoothedParam smoothedAsymmetry;

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, kNumOversamplers> oversamplers;
};
} // namespace milodikfx::dsp
