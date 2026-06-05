#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <vector>

#include "dsp/AudioProcessorBase.h"

namespace milodikfx::dsp
{
class EQProcessor final : public AudioProcessorBase
{
public:
    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels) override;
    void processBlock (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    void setBassDb (float db) noexcept;
    float getBassDb() const noexcept;

    void setMidDb (float db) noexcept;
    float getMidDb() const noexcept;

    void setTrebleDb (float db) noexcept;
    float getTrebleDb() const noexcept;

    void setEnabled (bool shouldEnable) noexcept;
    bool isEnabled() const noexcept;

private:
    void updateCoefficientsIfNeeded();

    std::atomic<float> bassDb { 0.0f };
    std::atomic<float> midDb { 0.0f };
    std::atomic<float> trebleDb { 0.0f };
    std::atomic<bool> enabled { true };

    float lastBassDb = 0.0f;
    float lastMidDb = 0.0f;
    float lastTrebleDb = 0.0f;

    double currentSampleRate = 0.0;
    int currentNumChannels = 0;

    juce::dsp::IIR::Coefficients<float>::Ptr bassCoefs;
    juce::dsp::IIR::Coefficients<float>::Ptr midCoefs;
    juce::dsp::IIR::Coefficients<float>::Ptr trebleCoefs;

    std::vector<juce::dsp::IIR::Filter<float>> bassFilters;
    std::vector<juce::dsp::IIR::Filter<float>> midFilters;
    std::vector<juce::dsp::IIR::Filter<float>> trebleFilters;

    bool prepared = false;
};
} // namespace milodikfx::dsp
