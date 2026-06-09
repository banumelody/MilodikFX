#include "WebServer.h"
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>

// Global Winsock initialization (one-time only)
static bool initWinsock()
{
    static bool initialized = false;
    static WSADATA wsaData;
    
    if (!initialized)
    {
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        initialized = (result == 0);
        if (!initialized)
        {
            juce::Logger::getCurrentLogger()->writeToLog(
                "CRITICAL: WSAStartup failed with error: " + juce::String(result));
        }
        else
        {
            juce::Logger::getCurrentLogger()->writeToLog(
                "Winsock2 initialized (version " + juce::String(MAKEWORD(2, 2)) + ")");
        }
    }
    return initialized;
}

WebServer::WebServer(int port)
    : juce::Thread("WebServerThread"),
      port_(port),
      running_(false),
      serverSocket_(INVALID_SOCKET)
{
    juce::Logger::getCurrentLogger()->writeToLog(
        "WebServer: Created instance on port " + juce::String(port));
    initWinsock();
}

WebServer::~WebServer()
{
    stop();
}

bool WebServer::setSocketNonBlocking(SOCKET sock, bool nonBlocking)
{
    unsigned long mode = nonBlocking ? 1 : 0;
    if (ioctlsocket(sock, FIONBIO, &mode) == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        juce::Logger::getCurrentLogger()->writeToLog(
            "WARN: ioctlsocket FIONBIO failed, error: " + juce::String(err));
        return false;
    }
    return true;
}

bool WebServer::start()
{
    juce::Logger::getCurrentLogger()->writeToLog(
        "WebServer::start() - Attempting to start server on port " + juce::String(port_));
    
    if (running_.load())
    {
        juce::Logger::getCurrentLogger()->writeToLog("WebServer already running");
        return true;
    }

    if (!initWinsock())
    {
        juce::Logger::getCurrentLogger()->writeToLog("CRITICAL: Winsock initialization failed");
        return false;
    }

    // Create server socket
    serverSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket_ == INVALID_SOCKET)
    {
        int err = WSAGetLastError();
        juce::Logger::getCurrentLogger()->writeToLog(
            "ERROR: socket() creation failed, error: " + juce::String(err));
        return false;
    }
    
    juce::Logger::getCurrentLogger()->writeToLog(
        "WebServer: Socket created (handle: " + juce::String((intptr_t)serverSocket_) + ")");

    // Enable SO_REUSEADDR to allow rapid rebinding after restart
    BOOL reuseAddr = TRUE;
    if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, 
                   (const char*)&reuseAddr, sizeof(reuseAddr)) == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        juce::Logger::getCurrentLogger()->writeToLog(
            "WARN: SO_REUSEADDR failed, error: " + juce::String(err));
    }
    else
    {
        juce::Logger::getCurrentLogger()->writeToLog("WebServer: SO_REUSEADDR enabled");
    }

    // Set socket to non-blocking for accept() with timeout
    if (!setSocketNonBlocking(serverSocket_, true))
    {
        juce::Logger::getCurrentLogger()->writeToLog("WARN: Non-blocking mode failed");
    }
    else
    {
        juce::Logger::getCurrentLogger()->writeToLog("WebServer: Non-blocking mode enabled");
    }

    // Bind socket to port
    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on all interfaces
    serverAddr.sin_port = htons(static_cast<u_short>(port_));

    if (bind(serverSocket_, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        juce::Logger::getCurrentLogger()->writeToLog(
            "ERROR: bind() failed on port " + juce::String(port_) + 
            ", error: " + juce::String(err));
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
        return false;
    }
    
    juce::Logger::getCurrentLogger()->writeToLog(
        "WebServer: Socket successfully bound to 0.0.0.0:" + juce::String(port_));

    // Listen for connections - use high backlog
    if (listen(serverSocket_, SOMAXCONN) == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        juce::Logger::getCurrentLogger()->writeToLog(
            "ERROR: listen() failed, error: " + juce::String(err));
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
        return false;
    }
    
    juce::Logger::getCurrentLogger()->writeToLog(
        "WebServer: Socket in LISTEN state, backlog=SOMAXCONN");
    
    running_.store(true);
    startThread();
    
    juce::Logger::getCurrentLogger()->writeToLog(
        "WebServer: Thread started, ready to accept connections on localhost:" + juce::String(port_));
    
    return true;
}

void WebServer::stop()
{
    juce::Logger::getCurrentLogger()->writeToLog("WebServer::stop() - Shutting down");
    
    running_.store(false);
    
    // Close the listening socket to interrupt accept()
    if (serverSocket_ != INVALID_SOCKET)
    {
        int result = closesocket(serverSocket_);
        if (result == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            juce::Logger::getCurrentLogger()->writeToLog(
                "WARN: closesocket() error: " + juce::String(err));
        }
        else
        {
            juce::Logger::getCurrentLogger()->writeToLog("WebServer: Listening socket closed");
        }
        serverSocket_ = INVALID_SOCKET;
    }
    
    // Wait for thread to exit
    if (isThreadRunning())
    {
        stopThread(5000);
    }
    
    // Wait for all connection threads to complete
    for (auto& thread : connectionThreads_)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }
    connectionThreads_.clear();
    
    juce::Logger::getCurrentLogger()->writeToLog("WebServer: Shutdown complete");
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

std::string WebServer::parseHttpRequest(const std::string& request, std::string& path, std::string& method) const
{
    std::istringstream stream(request);
    std::string requestPath, httpVersion;
    
    stream >> method >> requestPath >> httpVersion;
    
    // Parse path and query string
    size_t questionMarkPos = requestPath.find('?');
    if (questionMarkPos != std::string::npos)
    {
        path = requestPath.substr(0, questionMarkPos);
        return requestPath.substr(questionMarkPos + 1); // query string
    }
    else
    {
        path = requestPath;
        return ""; // no query string
    }
}

void WebServer::handleConnectionAsync(SOCKET clientSocket)
{
    // Run connection handling in a separate thread to prevent blocking accept()
    std::thread connThread([this, clientSocket]()
    {
        handleConnection(clientSocket);
    });
    
    // Add thread to vector for cleanup
    connectionThreads_.push_back(std::move(connThread));
    
    // Clean up finished threads periodically (remove joinable threads)
    std::vector<std::thread> activeThreads;
    for (auto& t : connectionThreads_)
    {
        if (t.joinable())
        {
            activeThreads.push_back(std::move(t));
        }
    }
    connectionThreads_ = std::move(activeThreads);
}

void WebServer::handleConnection(SOCKET clientSocket)
{
    if (clientSocket == INVALID_SOCKET)
        return;

    const int bufferSize = 8192;
    char buffer[bufferSize] = {};
    
    // Set a receive timeout (5 seconds)
    int timeoutMs = 5000;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, 
               (const char*)&timeoutMs, sizeof(timeoutMs));
    
    int bytesReceived = recv(clientSocket, buffer, bufferSize - 1, 0);
    
    if (bytesReceived <= 0)
    {
        int err = WSAGetLastError();
        if (err != WSAETIMEDOUT && err != 0)
        {
            juce::Logger::getCurrentLogger()->writeToLog(
                "WARN: recv() failed, error: " + juce::String(err));
        }
        closesocket(clientSocket);
        return;
    }
    
    buffer[bytesReceived] = '\0';
    std::string requestStr(buffer);
    
    // Parse HTTP request
    std::string filePath, method;
    std::string query = parseHttpRequest(requestStr, filePath, method);
    
    juce::Logger::getCurrentLogger()->writeToLog(
        "WebServer: HTTP " + juce::String(method) + " " + juce::String(filePath));
    
    // Extract request body for POST/PUT (simple parsing: skip headers, everything after \r\n\r\n)
    std::string requestBody;
    size_t bodyStart = requestStr.find("\r\n\r\n");
    if (bodyStart != std::string::npos)
    {
        requestBody = requestStr.substr(bodyStart + 4);
    }
    
    // Build HTTP response
    std::ostringstream response;
    std::string responseBody;
    std::string responseContentType = "text/html";
    int responseStatusCode = 200;
    
    // Check if this is a REST API request
    if (filePath.find("/api/") == 0)
    {
        juce::Logger::getCurrentLogger()->writeToLog(
            "WebServer: Routing REST API request: " + juce::String(method) + " " + juce::String(filePath));
        
        // Dispatch to REST API handler
        auto apiResponse = dispatcher_.dispatch(method, filePath, query, requestBody);
        responseStatusCode = apiResponse.statusCode;
        responseContentType = apiResponse.contentType;
        responseBody = apiResponse.body;
    }
    else
    {
        // Serve static file
        std::string contentType;
        bool fileFound = serveFile(filePath, contentType, responseBody);
        
        if (fileFound)
        {
            responseStatusCode = 200;
            responseContentType = contentType;
        }
        else
        {
            responseStatusCode = 404;
            responseContentType = "text/html";
            responseBody = "<!DOCTYPE html><html><body><h1>404 - File Not Found</h1>"
                          "<p>The requested file was not found.</p></body></html>";
            juce::Logger::getCurrentLogger()->writeToLog(
                "WebServer: File not found: " + juce::String(filePath));
        }
    }
    
    // Build HTTP response header
    std::string statusLine;
    switch (responseStatusCode)
    {
        case 200: statusLine = "HTTP/1.1 200 OK"; break;
        case 400: statusLine = "HTTP/1.1 400 Bad Request"; break;
        case 404: statusLine = "HTTP/1.1 404 Not Found"; break;
        case 405: statusLine = "HTTP/1.1 405 Method Not Allowed"; break;
        case 500: statusLine = "HTTP/1.1 500 Internal Server Error"; break;
        default: statusLine = "HTTP/1.1 200 OK"; break;
    }
    
    response << statusLine << "\r\n";
    response << "Content-Type: " << responseContentType << "\r\n";
    response << "Content-Length: " << responseBody.length() << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
    response << "Access-Control-Allow-Headers: Content-Type\r\n";
    response << "Cache-Control: no-cache\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << responseBody;
    
    std::string responseStr = response.str();
    const char* data = responseStr.c_str();
    int totalSize = (int)responseStr.length();
    int bytesSent = 0;
    
    // Send all response data
    while (bytesSent < totalSize && running_.load())
    {
        int sent = send(clientSocket, data + bytesSent, totalSize - bytesSent, 0);
        if (sent == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            juce::Logger::getCurrentLogger()->writeToLog(
                "WARN: send() error: " + juce::String(err));
            break;
        }
        bytesSent += sent;
    }
    
    // Graceful shutdown
    shutdown(clientSocket, SD_SEND);
    closesocket(clientSocket);
}

void WebServer::run()
{
    juce::Logger::getCurrentLogger()->writeToLog(
        "WebServer: Accept loop started on port " + juce::String(port_));
    
    while (running_.load() && serverSocket_ != INVALID_SOCKET)
    {
        sockaddr_in clientAddr = {};
        int clientAddrLen = sizeof(clientAddr);
        
        // Accept with non-blocking socket (times out and returns INVALID_SOCKET)
        SOCKET clientSocket = accept(serverSocket_, (sockaddr*)&clientAddr, &clientAddrLen);
        
        if (clientSocket == INVALID_SOCKET)
        {
            int err = WSAGetLastError();
            
            // WSAEWOULDBLOCK is normal for non-blocking socket when no connection is waiting
            if (err == WSAEWOULDBLOCK)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            // WSAEINTR occurs when the socket is being shut down
            if (err == WSAEINTR)
            {
                break;
            }
            
            if (running_.load())
            {
                juce::Logger::getCurrentLogger()->writeToLog(
                    "WARN: accept() error: " + juce::String(err));
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            continue;
        }
        
        // Get client IP for logging
        char clientIP[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
        juce::Logger::getCurrentLogger()->writeToLog(
            "WebServer: Connection accepted from " + juce::String(clientIP) + 
            ":" + juce::String(ntohs(clientAddr.sin_port)));
        
        // Handle connection asynchronously to keep accept() loop responsive
        handleConnectionAsync(clientSocket);
    }
    
    juce::Logger::getCurrentLogger()->writeToLog("WebServer: Accept loop exited");
}

