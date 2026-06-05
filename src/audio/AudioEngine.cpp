#include "audio/AudioEngine.h"

namespace milodikfx::audio
{
void AudioEngine::prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels)
{
    chain.prepareToPlay (sampleRate, samplesPerBlock, numChannels);
}

void AudioEngine::processBlock (juce::AudioBuffer<float>& buffer)
{
    chain.processBlock (buffer);
}

void AudioEngine::reset()
{
    chain.reset();
}

void AudioEngine::setBypassed (bool shouldBypass) noexcept
{
    chain.setBypassed (shouldBypass);
}

bool AudioEngine::isBypassed() const noexcept
{
    return chain.isBypassed();
}

milodikfx::dsp::DSPChainManager& AudioEngine::getChain() noexcept
{
    return chain;
}
} // namespace milodikfx::audio
