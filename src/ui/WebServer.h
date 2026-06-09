#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <string>
#include <memory>
#include <map>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <vector>

#include "../api/RestApiDispatcher.h"
#include "../api/HttpHandler.h"

#pragma comment(lib, "Ws2_32.lib")

/**
 * Reliable HTTP server for serving embedded web UI using Windows Winsock2 API.
 * Serves static files (HTML, CSS, JS) from a resources directory.
 * 
 * Key improvements:
 * - Proper SO_REUSEADDR and socket initialization
 * - Non-blocking listening socket with timeout
 * - Per-connection thread handling (no blocking on main accept loop)
 * - Robust error handling and logging
 * - Proper socket state management
 */
class WebServer : private juce::Thread
{
public:
    explicit WebServer(int port = 3000);
    ~WebServer();
    
    bool start();
    void stop();
    bool isRunning() const;
    
    int getPort() const { return port_; }
    
    /**
     * Reads a file from the resources directory.
     * @param filePath The requested path (e.g., "/index.html" or "/assets/index-*.js")
     * @param contentType Output parameter for MIME type
     * @param body Output parameter for file content
     * @return true if file was found and read
     */
    bool serveFile(const std::string& filePath, std::string& contentType, std::string& body) const;

    /**
     * Register a REST API handler for a path
     * @param pathPrefix e.g., "/api/devices"
     * @param handler The handler to handle requests for this path
     */
    void registerApiHandler(const std::string& pathPrefix, std::shared_ptr<HttpHandler> handler)
    {
        dispatcher_.registerHandler(pathPrefix, handler);
    }

private:
    int port_;
    std::atomic<bool> running_;
    SOCKET serverSocket_;
    std::vector<std::thread> connectionThreads_;
    RestApiDispatcher dispatcher_;
    
    void run() override;
    std::string getMimeType(const std::string& filePath) const;
    std::string getResourceDirectory() const;
    void handleConnectionAsync(SOCKET clientSocket);
    void handleConnection(SOCKET clientSocket);
    std::string parseHttpRequest(const std::string& request, std::string& path, std::string& method) const;
    bool setSocketNonBlocking(SOCKET sock, bool nonBlocking);
};

