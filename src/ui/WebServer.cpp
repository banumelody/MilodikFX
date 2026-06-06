#include "WebServer.h"
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>

// Initialize Winsock on first use
static bool initWinsock()
{
    static bool initialized = false;
    if (!initialized)
    {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        initialized = (result == 0);
        if (!initialized)
        {
            juce::Logger::getCurrentLogger()->writeToLog("WSAStartup failed");
        }
    }
    return initialized;
}

WebServer::WebServer(int port)
    : juce::Thread("WebServerThread"), port_(port), running_(false), serverSocket_(INVALID_SOCKET)
{
    juce::Logger::getCurrentLogger()->writeToLog("WebServer created on port " + juce::String(port));
    initWinsock();
}

WebServer::~WebServer()
{
    stop();
}

bool WebServer::start()
{
    juce::Logger::getCurrentLogger()->writeToLog("WebServer::start() called for port " + juce::String(port_));
    
    if (running_.load()) {
        juce::Logger::getCurrentLogger()->writeToLog("WebServer already running");
        return true;
    }

    if (!initWinsock())
    {
        juce::Logger::getCurrentLogger()->writeToLog("Failed to initialize Winsock");
        return false;
    }

    // Create server socket
    serverSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket_ == INVALID_SOCKET)
    {
        juce::Logger::getCurrentLogger()->writeToLog("Failed to create socket, error: " + juce::String(WSAGetLastError()));
        return false;
    }
    
    juce::Logger::getCurrentLogger()->writeToLog("Socket created successfully");

    // Set SO_REUSEADDR to allow quick restart
    BOOL reuseAddr = TRUE;
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseAddr, sizeof(reuseAddr));

    // Bind socket to port
    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on all interfaces
    serverAddr.sin_port = htons(port_);

    if (bind(serverSocket_, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        juce::Logger::getCurrentLogger()->writeToLog("bind() failed, error: " + juce::String(err));
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
        return false;
    }
    
    juce::Logger::getCurrentLogger()->writeToLog("Socket bound to port " + juce::String(port_));

    // Listen for connections
    if (listen(serverSocket_, SOMAXCONN) == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        juce::Logger::getCurrentLogger()->writeToLog("listen() failed, error: " + juce::String(err));
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
        return false;
    }
    
    juce::Logger::getCurrentLogger()->writeToLog("Socket listening on port " + juce::String(port_));
    
    running_.store(true);
    startThread();
    
    juce::Logger::getCurrentLogger()->writeToLog("WebServer thread started");
    
    return true;
}

void WebServer::stop()
{
    running_.store(false);
    
    if (serverSocket_ != INVALID_SOCKET)
    {
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
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
    auto fullPath = resourceDir.getFullPathName();
    
    juce::Logger::getCurrentLogger()->writeToLog(
        "WebServer: Resource directory = " + fullPath);
    
    return fullPath.toStdString();
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
    // Navigate to ui/web subdirectory
    auto webDir = resourceFile.getChildFile("ui").getChildFile("web");
    auto targetFile = webDir.getChildFile(juce::String(sanitizedPath));
    
    if (!targetFile.existsAsFile())
    {
        juce::Logger::getCurrentLogger()->writeToLog(
            "WebServer: File not found: " + juce::String(sanitizedPath) + 
            " (searched in " + webDir.getFullPathName() + ")");
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

void WebServer::handleConnection(SOCKET clientSocket)
{
    const int bufferSize = 4096;
    char buffer[bufferSize] = {};
    
    int bytesReceived = recv(clientSocket, buffer, bufferSize - 1, 0);
    
    if (bytesReceived <= 0)
    {
        closesocket(clientSocket);
        return;
    }
    
    buffer[bytesReceived] = '\0';
    std::string requestStr(buffer);
    
    std::string filePath;
    std::string method = parseHttpRequest(requestStr, filePath);
    
    juce::Logger::getCurrentLogger()->writeToLog(
        "WebServer: HTTP " + juce::String(method) + " " + juce::String(filePath));
    
    std::string contentType, body;
    bool fileFound = serveFile(filePath, contentType, body);
    
    if (!fileFound) {
        juce::Logger::getCurrentLogger()->writeToLog(
            "WebServer: File not found: " + juce::String(filePath));
    }
    
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
    const char* data = responseStr.c_str();
    int totalSize = (int)responseStr.length();
    int bytesSent = 0;
    
    // Keep sending until all data is transmitted
    while (bytesSent < totalSize)
    {
        int sent = send(clientSocket, data + bytesSent, totalSize - bytesSent, 0);
        if (sent == SOCKET_ERROR)
        {
            break;
        }
        bytesSent += sent;
    }
    
    closesocket(clientSocket);
}

void WebServer::run()
{
    juce::Logger::getCurrentLogger()->writeToLog("WebServer thread running, accepting connections on port " + juce::String(port_));
    
    while (running_.load() && serverSocket_ != INVALID_SOCKET)
    {
        sockaddr_in clientAddr = {};
        int clientAddrLen = sizeof(clientAddr);
        
        // Accept incoming connection with timeout
        SOCKET clientSocket = accept(serverSocket_, (sockaddr*)&clientAddr, &clientAddrLen);
        
        if (clientSocket == INVALID_SOCKET)
        {
            if (running_.load())
            {
                // Only log if we're still supposed to be running
                int err = WSAGetLastError();
                if (err != WSAEINTR)  // Ignore "Interrupted" errors
                {
                    // Small sleep to avoid busy-waiting on errors
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
            continue;
        }
        
        handleConnection(clientSocket);
    }
    
    juce::Logger::getCurrentLogger()->writeToLog("WebServer thread exiting");
}

