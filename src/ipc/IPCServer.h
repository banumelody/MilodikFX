#pragma once

#include <JuceHeader.h>
#include <functional>
#include <map>
#include <memory>
#include <thread>
#include <mutex>
#include <queue>
#include "ipc/Types.h"

namespace milodikfx::ipc
{

// ============================================================================
// IPC Server for Frontend-Backend Communication (Simplified)
// ============================================================================

class IPCServer : public juce::Thread
{
public:
    using MessageCallback = std::function<void(const juce::var&)>;

    explicit IPCServer(int port = 9999);
    ~IPCServer() override;

    // Server lifecycle
    bool start();
    void stop();
    bool isRunning() const;

    // Message sending to frontend
    void sendMessage(MessageType type, const juce::var& payload);
    void broadcastMessage(MessageType type, const juce::var& payload);

    // Message handler registration
    void registerHandler(MessageType type, MessageCallback handler);

    // Get server port
    int getPort() const { return port_; }

private:
    void run() override;
    void processMessageQueue();

    int port_;
    bool running_ = false;

    struct PendingMessage
    {
        MessageType type;
        juce::var payload;
    };

    std::queue<PendingMessage> messageQueue_;
    mutable std::mutex queueMutex_;

    std::map<MessageType, MessageCallback> handlers_;
    mutable std::mutex handlerMutex_;
};

} // namespace milodikfx::ipc
