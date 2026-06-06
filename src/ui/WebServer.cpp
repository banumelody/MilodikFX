#include "WebServer.h"
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>

WebServer::WebServer(int port)
    : juce::Thread("WebServerThread"), port_(port), running_(false), serverSocket_(nullptr)
{
}

WebServer::~WebServer()
{
    stop();
}

bool WebServer::start()
{
    if (running_.load())
        return true;

    // Create server socket
    serverSocket_ = std::make_unique<juce::StreamingSocket>();
    
    if (!serverSocket_->createListener(port_))
    {
        juce::Logger::getCurrentLogger()->writeToLog(
            "WebServer: Failed to bind to port " + std::to_string(port_));
        serverSocket_.reset();
        return false;
    }
    
    running_.store(true);
    startThread();
    
    juce::Logger::getCurrentLogger()->writeToLog(
        "WebServer: Started on port " + std::to_string(port_));
    
    return true;
}

void WebServer::stop()
{
    running_.store(false);
    
    if (serverSocket_)
    {
        serverSocket_->close();
        serverSocket_.reset();
    }
    
    if (isThreadRunning())
    {
        stopThread(5000);
    }
}

bool WebServer::isRunning() const
{
    return running_.load();
}

std::string WebServer::getMimeType(const std::string& filePath) const
{
    if (filePath.find(".html") != std::string::npos)
        return "text/html; charset=utf-8";
    else if (filePath.find(".js") != std::string::npos)
        return "application/javascript; charset=utf-8";
    else if (filePath.find(".css") != std::string::npos)
        return "text/css; charset=utf-8";
    else if (filePath.find(".json") != std::string::npos)
        return "application/json; charset=utf-8";
    else if (filePath.find(".svg") != std::string::npos)
        return "image/svg+xml";
    else if (filePath.find(".png") != std::string::npos)
        return "image/png";
    else if (filePath.find(".jpg") != std::string::npos || filePath.find(".jpeg") != std::string::npos)
        return "image/jpeg";
    else if (filePath.find(".gif") != std::string::npos)
        return "image/gif";
    else if (filePath.find(".woff2") != std::string::npos)
        return "font/woff2";
    
    return "application/octet-stream";
}

std::string WebServer::getResourceDirectory() const
{
    auto exePath = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    auto resourceDir = exePath.getParentDirectory().getChildFile("resources");
    return resourceDir.getFullPathName().toStdString();
}

bool WebServer::serveFile(const std::string& filePath, std::string& contentType, std::string& body) const
{
    std::string sanitizedPath = filePath;
    
    // Security: prevent directory traversal
    if (sanitizedPath.find("..") != std::string::npos)
        return false;
    
    // Default to index.html for root
    if (sanitizedPath == "/" || sanitizedPath.empty())
        sanitizedPath = "/index.html";
    
    // Remove leading slash for file lookup
    if (sanitizedPath[0] == '/')
        sanitizedPath = sanitizedPath.substr(1);
    
    auto resourceDir = getResourceDirectory();
    juce::File resourceFile(resourceDir);
    auto targetFile = resourceFile.getChildFile(juce::String(sanitizedPath));
    
    if (!targetFile.existsAsFile())
    {
        juce::Logger::getCurrentLogger()->writeToLog(
            "WebServer: File not found: " + sanitizedPath);
        return false;
    }
    
    body = targetFile.loadFileAsString().toStdString();
    contentType = getMimeType(filePath);
    
    return true;
}

std::string WebServer::parseHttpRequest(const std::string& request, std::string& path) const
{
    std::istringstream stream(request);
    std::string method, requestPath, httpVersion;
    
    stream >> method >> requestPath >> httpVersion;
    
    path = requestPath;
    return method;
}

void WebServer::handleConnection(juce::StreamingSocket* socket)
{
    if (!socket || !socket->isConnected())
        return;
    
    const int bufferSize = 4096;
    char buffer[bufferSize];
    
    int bytesRead = socket->read(buffer, bufferSize - 1, false);
    
    if (bytesRead <= 0)
        return;
    
    buffer[bytesRead] = '\0';
    std::string requestStr(buffer);
    
    std::string filePath;
    std::string method = parseHttpRequest(requestStr, filePath);
    
    std::string contentType, body;
    bool fileFound = serveFile(filePath, contentType, body);
    
    // Build HTTP response
    std::ostringstream response;
    
    if (fileFound)
    {
        response << "HTTP/1.1 200 OK\r\n";
        response << "Content-Type: " << contentType << "\r\n";
        response << "Content-Length: " << body.length() << "\r\n";
        response << "Access-Control-Allow-Origin: *\r\n";
        response << "Cache-Control: no-cache\r\n";
        response << "Connection: close\r\n";
        response << "\r\n";
        response << body;
    }
    else
    {
        std::string notFoundBody = "<!DOCTYPE html><html><body><h1>404 - File Not Found</h1>"
                                   "<p>The requested file was not found.</p></body></html>";
        response << "HTTP/1.1 404 Not Found\r\n";
        response << "Content-Type: text/html\r\n";
        response << "Content-Length: " << notFoundBody.length() << "\r\n";
        response << "Connection: close\r\n";
        response << "\r\n";
        response << notFoundBody;
    }
    
    std::string responseStr = response.str();
    socket->write(responseStr.c_str(), static_cast<int>(responseStr.length()));
}

void WebServer::run()
{
    while (running_.load() && serverSocket_ && serverSocket_->isConnected())
    {
        auto socket = serverSocket_->waitForNextConnection();
        
        if (socket)
        {
            handleConnection(socket);
        }
        
        // Small sleep to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

