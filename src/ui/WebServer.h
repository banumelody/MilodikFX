#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <string>
#include <memory>
#include <map>

/**
 * Simple HTTP server for serving embedded web UI.
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
    std::unique_ptr<juce::StreamingSocket> serverSocket_;
    
    void run() override;
    std::string getMimeType(const std::string& filePath) const;
    std::string getResourceDirectory() const;
    void handleConnection(juce::StreamingSocket* socket);
    std::string parseHttpRequest(const std::string& request, std::string& path) const;
};

