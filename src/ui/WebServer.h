#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <string>
#include <memory>
#include <map>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

/**
 * Simple HTTP server for serving embedded web UI using Windows socket API.
 * Serves static files (HTML, CSS, JS) from a resources directory.
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

private:
    int port_;
    std::atomic<bool> running_;
    SOCKET serverSocket_;
    
    void run() override;
    std::string getMimeType(const std::string& filePath) const;
    std::string getResourceDirectory() const;
    void handleConnection(SOCKET clientSocket);
    std::string parseHttpRequest(const std::string& request, std::string& path) const;
};

