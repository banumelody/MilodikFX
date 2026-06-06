#include "ipc/MetronomeBridge.h"

namespace milodikfx::ipc
{

MetronomeBridge::MetronomeBridge()
    : lastTapTime_(std::chrono::steady_clock::now()), currentTempo_(120.0f)
{
}

float MetronomeBridge::registerTap()
{
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - lastTapTime_);

    float intervalMs = static_cast<float>(duration.count());
    lastTapTime_ = now;

    if (intervalMs < 100.0f)
        return currentTempo_; // Too fast, ignore

    tapIntervals_.push_back(intervalMs);

    if (tapIntervals_.size() > MAX_TAP_HISTORY)
        tapIntervals_.erase(tapIntervals_.begin());

    // Calculate average tempo
    if (tapIntervals_.size() >= 2)
    {
        float avgInterval = 0.0f;
        for (float interval : tapIntervals_)
            avgInterval += interval;
        avgInterval /= tapIntervals_.size();

        // Convert interval (ms per beat) to BPM (beats per minute)
        currentTempo_ = 60000.0f / avgInterval;
        currentTempo_ = std::max(MIN_TEMPO, std::min(MAX_TEMPO, currentTempo_));
    }

    return currentTempo_;
}

float MetronomeBridge::getCurrentTempo() const
{
    return currentTempo_;
}

void MetronomeBridge::setTempo(float bpm)
{
    currentTempo_ = std::max(MIN_TEMPO, std::min(MAX_TEMPO, bpm));
}

void MetronomeBridge::resetTapTempo()
{
    tapIntervals_.clear();
    lastTapTime_ = std::chrono::steady_clock::now();
}

float MetronomeBridge::getDetectedTempo() const
{
    return currentTempo_;
}

} // namespace milodikfx::ipc
