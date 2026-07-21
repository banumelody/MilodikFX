#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <memory>
#include <string>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "../api/HttpHandler.h"
#include "../api/RestApiDispatcher.h"

#pragma comment(lib, "Ws2_32.lib")

/**
 * Minimal HTTP server for the local UI and REST API.
 *
 * Binds to the loopback interface only: this endpoint can switch audio
 * hardware and write preset files, so it must not be reachable from the network.
 * Each connection is handled on its own detached thread, and stop() waits for
 * the in-flight ones so a handler cannot outlive the objects it references.
 */
class WebServer : private juce::Thread
{
public:
    explicit WebServer (int port = 3000);
    ~WebServer() override;

    bool start();
    void stop();
    bool isRunning() const;

    int getPort() const { return port_; }

    /**
     * Reads a file from <exe dir>/resources/ui/web.
     * @return false if the path escapes the web root or the file is missing.
     */
    bool serveFile (const std::string& filePath, std::string& contentType, std::string& body) const;

    void registerApiHandler (const std::string& pathPrefix, std::shared_ptr<HttpHandler> handler)
    {
        dispatcher_.registerHandler (pathPrefix, handler);
    }

private:
    // Anything larger than this is not a control message we asked for.
    static constexpr int kMaxRequestBodyBytes = 1 * 1024 * 1024;
    static constexpr int kMaxHeaderBytes = 32 * 1024;

    int port_;
    std::atomic<bool> running_ { false };
    std::atomic<int> activeConnections_ { 0 };
    SOCKET serverSocket_ = INVALID_SOCKET;
    RestApiDispatcher dispatcher_;

    void run() override;

    std::string getMimeType (const std::string& filePath) const;
    juce::File getWebRoot() const;
    std::string buildFallbackPage() const;

    void handleConnection (SOCKET clientSocket);
    std::string parseHttpRequest (const std::string& request, std::string& path, std::string& method) const;

    static bool setSocketNonBlocking (SOCKET sock, bool nonBlocking);
    static void sendAll (SOCKET sock, const std::string& payload);
};
