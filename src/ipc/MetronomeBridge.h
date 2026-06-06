#pragma once

#include <JuceHeader.h>
#include <chrono>

namespace milodikfx::ipc
{

class MetronomeBridge
{
public:
    MetronomeBridge();

    // Register tap
    float registerTap();

    // Get current tempo
    float getCurrentTempo() const;

    // Set tempo manually
    void setTempo(float bpm);

    // Reset tap tempo
    void resetTapTempo();

    // Get detected tempo
    float getDetectedTempo() const;

private:
    static constexpr int MAX_TAP_HISTORY = 8;
    static constexpr float MIN_TEMPO = 30.0f;
    static constexpr float MAX_TEMPO = 300.0f;

    std::chrono::steady_clock::time_point lastTapTime_;
    std::vector<float> tapIntervals_;
    float currentTempo_ = 120.0f;
};

} // namespace milodikfx::ipc
