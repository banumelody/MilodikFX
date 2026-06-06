#include "ipc/IPCServer.h"
#include <chrono>

namespace milodikfx::ipc
{

IPCServer::IPCServer(int port)
    : Thread("IPCServer"), port_(port)
{
}

IPCServer::~IPCServer()
{
    stop();
}

bool IPCServer::start()
{
    if (running_)
        return true;

    running_ = true;
    startThread(juce::Thread::Priority::normal);

    DBG("IPC Server initialized on port " << port_);
    return true;
}

void IPCServer::stop()
{
    if (!running_)
        return;

    running_ = false;
    stopThread(1000);
    DBG("IPC Server stopped");
}

bool IPCServer::isRunning() const
{
    return running_;
}

void IPCServer::run()
{
    while (running_)
    {
        processMessageQueue();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void IPCServer::processMessageQueue()
{
    std::lock_guard<std::mutex> lock(queueMutex_);

    while (!messageQueue_.empty())
    {
        auto message = messageQueue_.front();
        messageQueue_.pop();

        auto msgType = messageTypeToString(message.type);
        auto jsonStr = juce::JSON::toString(message.payload);

        DBG("IPC: " << msgType << " -> " << jsonStr);
    }
}

void IPCServer::sendMessage(MessageType type, const juce::var& payload)
{
    std::lock_guard<std::mutex> lock(queueMutex_);
    messageQueue_.push({type, payload});
}

void IPCServer::broadcastMessage(MessageType type, const juce::var& payload)
{
    sendMessage(type, payload);
}

void IPCServer::registerHandler(MessageType type, MessageCallback handler)
{
    std::lock_guard<std::mutex> lock(handlerMutex_);
    handlers_[type] = handler;
}

} // namespace milodikfx::ipc

