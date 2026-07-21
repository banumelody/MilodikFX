#include "WebServer.h"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <thread>

#if MILODIKFX_EMBED_UI
 #include "MilodikFXUiData.h"
#endif

namespace
{
#if MILODIKFX_EMBED_UI
/**
 * Looks a file up in the bundle baked into the exe.
 *
 * Matching is by original filename rather than by JUCE's mangled symbol name,
 * so renaming a bundle file cannot silently stop it being found.
 */
bool findEmbeddedResource (const std::string& fileName, const char*& data, int& size)
{
    for (int i = 0; i < MilodikFXUiData::namedResourceListSize; ++i)
    {
        if (fileName == MilodikFXUiData::originalFilenames[i])
        {
            int resourceSize = 0;
            data = MilodikFXUiData::getNamedResource (MilodikFXUiData::namedResourceList[i], resourceSize);
            size = resourceSize;
            return data != nullptr && resourceSize > 0;
        }
    }

    return false;
}
#endif

void log (const juce::String& message)
{
    // The logger is installed and removed by the application, so a connection
    // thread finishing during shutdown can legitimately find none.
    if (auto* logger = juce::Logger::getCurrentLogger())
        logger->writeToLog (message);
}

bool initWinsock()
{
    static bool initialised = []
    {
        WSADATA data {};
        const auto result = WSAStartup (MAKEWORD (2, 2), &data);

        if (result != 0)
            log ("CRITICAL: WSAStartup failed with error " + juce::String (result));

        return result == 0;
    }();

    return initialised;
}

std::string toLower (std::string s)
{
    std::transform (s.begin(), s.end(), s.begin(), [] (unsigned char c) { return (char) std::tolower (c); });
    return s;
}

const char* statusText (int code)
{
    switch (code)
    {
        case 200: return "200 OK";
        case 204: return "204 No Content";
        case 400: return "400 Bad Request";
        case 403: return "403 Forbidden";
        case 404: return "404 Not Found";
        case 405: return "405 Method Not Allowed";
        case 413: return "413 Payload Too Large";
        case 500: return "500 Internal Server Error";
        case 503: return "503 Service Unavailable";
        default: return "200 OK";
    }
}
} // namespace

WebServer::WebServer (int port)
    : juce::Thread ("MilodikFX WebServer"),
      port_ (port)
{
    initWinsock();
}

WebServer::~WebServer()
{
    stop();
}

bool WebServer::setSocketNonBlocking (SOCKET sock, bool nonBlocking)
{
    unsigned long mode = nonBlocking ? 1 : 0;
    return ioctlsocket (sock, FIONBIO, &mode) != SOCKET_ERROR;
}

bool WebServer::start()
{
    if (running_.load())
        return true;

    if (! initWinsock())
        return false;

    serverSocket_ = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (serverSocket_ == INVALID_SOCKET)
    {
        log ("WebServer: socket() failed, error " + juce::String (WSAGetLastError()));
        return false;
    }

    // Deliberately NOT SO_REUSEADDR: on Windows that lets two processes bind
    // the same port, which would silently split requests between two engines.
    if (! setSocketNonBlocking (serverSocket_, true))
        log ("WebServer: could not set non-blocking mode");

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
    address.sin_port = htons ((u_short) port_);

    if (bind (serverSocket_, (sockaddr*) &address, sizeof (address)) == SOCKET_ERROR)
    {
        closesocket (serverSocket_);
        serverSocket_ = INVALID_SOCKET;
        return false;
    }

    if (listen (serverSocket_, SOMAXCONN) == SOCKET_ERROR)
    {
        log ("WebServer: listen() failed, error " + juce::String (WSAGetLastError()));
        closesocket (serverSocket_);
        serverSocket_ = INVALID_SOCKET;
        return false;
    }

    running_.store (true);
    startThread();

    log ("WebServer: listening on 127.0.0.1:" + juce::String (port_));
    return true;
}

void WebServer::stop()
{
    if (! running_.exchange (false) && serverSocket_ == INVALID_SOCKET)
        return;

    if (serverSocket_ != INVALID_SOCKET)
    {
        closesocket (serverSocket_);
        serverSocket_ = INVALID_SOCKET;
    }

    if (isThreadRunning())
        stopThread (5000);

    // Connection threads hold raw references to the handlers, which the owner
    // destroys right after this returns, so wait them out rather than hoping.
    const auto deadline = juce::Time::getMillisecondCounter() + 5000;

    while (activeConnections_.load() > 0 && juce::Time::getMillisecondCounter() < deadline)
        std::this_thread::sleep_for (std::chrono::milliseconds (5));

    if (const auto remaining = activeConnections_.load(); remaining > 0)
        log ("WebServer: " + juce::String (remaining) + " connection(s) still active at shutdown");

    log ("WebServer: stopped");
}

bool WebServer::isRunning() const
{
    return running_.load();
}

std::string WebServer::getMimeType (const std::string& filePath) const
{
    const auto path = toLower (filePath);

    auto endsWith = [&path] (const char* suffix)
    {
        const std::string s (suffix);
        return path.size() >= s.size() && path.compare (path.size() - s.size(), s.size(), s) == 0;
    };

    if (endsWith (".html") || endsWith (".htm")) return "text/html; charset=utf-8";
    if (endsWith (".js") || endsWith (".mjs"))   return "application/javascript; charset=utf-8";
    if (endsWith (".css"))                       return "text/css; charset=utf-8";
    if (endsWith (".json"))                      return "application/json; charset=utf-8";
    if (endsWith (".svg"))                       return "image/svg+xml";
    if (endsWith (".png"))                       return "image/png";
    if (endsWith (".jpg") || endsWith (".jpeg")) return "image/jpeg";
    if (endsWith (".gif"))                       return "image/gif";
    if (endsWith (".webp"))                      return "image/webp";
    if (endsWith (".ico"))                       return "image/x-icon";
    if (endsWith (".woff2"))                     return "font/woff2";
    if (endsWith (".woff"))                      return "font/woff";
    if (endsWith (".ttf"))                       return "font/ttf";
    if (endsWith (".map"))                       return "application/json; charset=utf-8";

    return "application/octet-stream";
}

juce::File WebServer::getWebRoot() const
{
    return juce::File::getSpecialLocation (juce::File::currentExecutableFile)
        .getParentDirectory()
        .getChildFile ("resources")
        .getChildFile ("ui")
        .getChildFile ("web");
}

bool WebServer::serveFile (const std::string& filePath, std::string& contentType, std::string& body) const
{
    auto requested = filePath;

    if (requested.empty() || requested == "/")
        requested = "/index.html";

    // Reject anything that could address a location rather than a relative name:
    // "..", a drive letter, a UNC prefix, or a NUL splice.
    if (requested.find ("..") != std::string::npos
        || requested.find (':') != std::string::npos
        || requested.find ('\\') != std::string::npos
        || requested.find ('\0') != std::string::npos)
        return false;

    while (! requested.empty() && requested.front() == '/')
        requested.erase (requested.begin());

    if (requested.empty())
        return false;

    const auto webRoot = getWebRoot();
    const auto target = webRoot.getChildFile (juce::String (requested));

    // Belt and braces: even if the checks above missed something, the resolved
    // path must still sit inside the web root.
    if (target.isAChildOf (webRoot) && target.existsAsFile())
    {
        juce::MemoryBlock contents;

        if (target.loadFileAsData (contents))
        {
            // Read as bytes, not as text: a font or image would be mangled by
            // any encoding conversion on the way through.
            body.assign (static_cast<const char*> (contents.getData()), contents.getSize());
            contentType = getMimeType (requested);
            return true;
        }
    }

   #if MILODIKFX_EMBED_UI
    // Nothing on disk, so fall back to the copy baked into the exe. This is
    // what lets the executable run on its own, with no resources folder.
    {
        const auto slash = requested.find_last_of ('/');
        const auto fileName = slash == std::string::npos ? requested : requested.substr (slash + 1);

        const char* data = nullptr;
        int size = 0;

        if (findEmbeddedResource (fileName, data, size))
        {
            body.assign (data, (size_t) size);
            contentType = getMimeType (fileName);
            return true;
        }
    }
   #endif

    return false;
}

std::string WebServer::buildFallbackPage() const
{
    return "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>MilodikFX</title>"
           "<style>body{background:#0d0f14;color:#e6e9f0;font:15px/1.6 Segoe UI,system-ui,sans-serif;"
           "display:flex;align-items:center;justify-content:center;height:100vh;margin:0}"
           "div{max-width:560px;padding:32px}h1{font-size:20px;margin:0 0 12px}"
           "code{background:#1b1f2a;padding:2px 6px;border-radius:4px}</style></head><body><div>"
           "<h1>UI belum ter-build</h1>"
           "<p>Engine audio berjalan, tetapi berkas antarmuka tidak ditemukan di "
           "<code>resources/ui/web</code>.</p>"
           "<p>Build frontend lalu jalankan ulang:</p>"
           "<p><code>cd frontend &amp;&amp; npm ci &amp;&amp; npm run build</code></p>"
           "</div></body></html>";
}

std::string WebServer::parseHttpRequest (const std::string& request, std::string& path, std::string& method) const
{
    std::istringstream stream (request);
    std::string requestPath, httpVersion;

    stream >> method >> requestPath >> httpVersion;

    const auto questionMark = requestPath.find ('?');

    if (questionMark != std::string::npos)
    {
        path = requestPath.substr (0, questionMark);
        return requestPath.substr (questionMark + 1);
    }

    path = requestPath;
    return {};
}

void WebServer::sendAll (SOCKET sock, const std::string& payload)
{
    int sent = 0;
    const auto total = (int) payload.size();

    while (sent < total)
    {
        const auto n = send (sock, payload.data() + sent, total - sent, 0);

        if (n == SOCKET_ERROR || n <= 0)
            return;

        sent += n;
    }
}

void WebServer::handleConnection (SOCKET clientSocket)
{
    if (clientSocket == INVALID_SOCKET)
        return;

    setSocketNonBlocking (clientSocket, false);

    int timeoutMs = 5000;
    setsockopt (clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*) &timeoutMs, sizeof (timeoutMs));
    setsockopt (clientSocket, SOL_SOCKET, SO_SNDTIMEO, (const char*) &timeoutMs, sizeof (timeoutMs));

    constexpr int bufferSize = 8192;
    char buffer[bufferSize] {};

    std::string request;
    size_t headerEnd = std::string::npos;

    while ((int) request.size() < kMaxHeaderBytes)
    {
        const auto received = recv (clientSocket, buffer, bufferSize, 0);

        if (received <= 0)
            break;

        request.append (buffer, (size_t) received);
        headerEnd = request.find ("\r\n\r\n");

        if (headerEnd != std::string::npos)
            break;
    }

    if (headerEnd == std::string::npos)
    {
        closesocket (clientSocket);
        return;
    }

    std::string path, method;
    const auto query = parseHttpRequest (request, path, method);

    const auto headers = toLower (request.substr (0, headerEnd));

    long long contentLength = 0;

    if (const auto pos = headers.find ("content-length:"); pos != std::string::npos)
    {
        const auto valueStart = pos + std::string ("content-length:").size();
        const auto lineEnd = headers.find ("\r\n", valueStart);
        const auto text = headers.substr (valueStart, lineEnd == std::string::npos ? std::string::npos
                                                                                  : lineEnd - valueStart);
        try
        {
            contentLength = std::stoll (text);
        }
        catch (...)
        {
            contentLength = 0;
        }
    }

    int responseStatus = 200;
    std::string responseContentType = "application/json";
    std::string responseBody;
    std::string extraHeaders;

    if (contentLength < 0 || contentLength > kMaxRequestBodyBytes)
    {
        // Refuse before allocating: an attacker-chosen Content-Length used to be
        // enough to make the server grow a string without bound.
        responseStatus = 413;
        responseBody = R"({"error":"Request body too large"})";
    }
    else
    {
        auto body = request.substr (headerEnd + 4);
        body.reserve ((size_t) contentLength);

        while ((long long) body.size() < contentLength)
        {
            const auto want = (int) std::min<long long> (bufferSize, contentLength - (long long) body.size());
            const auto received = recv (clientSocket, buffer, want, 0);

            if (received <= 0)
                break;

            body.append (buffer, (size_t) received);
        }

        if (method == "OPTIONS")
        {
            // Preflight used to fall through to 405, which broke every
            // cross-origin request the browser tried to make.
            responseStatus = 204;
            responseContentType = "text/plain";
        }
        else if (path.rfind ("/api/", 0) == 0)
        {
            const auto apiResponse = dispatcher_.dispatch (method, path, query, body);
            responseStatus = apiResponse.statusCode;
            responseContentType = apiResponse.contentType;
            responseBody = apiResponse.body;
        }
        else if (method != "GET" && method != "HEAD")
        {
            responseStatus = 405;
            responseBody = R"({"error":"Method Not Allowed"})";
        }
        else
        {
            std::string contentType;

            if (serveFile (path, contentType, responseBody))
            {
                responseContentType = contentType;
            }
            else if (path == "/" || path == "/index.html")
            {
                responseStatus = 200;
                responseContentType = "text/html; charset=utf-8";
                responseBody = buildFallbackPage();
            }
            else
            {
                responseStatus = 404;
                responseContentType = "text/plain; charset=utf-8";
                responseBody = "Not found";
            }
        }
    }

    std::ostringstream response;

    response << "HTTP/1.1 " << statusText (responseStatus) << "\r\n"
             << "Content-Type: " << responseContentType << "\r\n"
             << "Content-Length: " << responseBody.size() << "\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
             << "Access-Control-Allow-Headers: Content-Type\r\n"
             << "X-Content-Type-Options: nosniff\r\n"
             << "Cache-Control: no-store\r\n"
             << "Connection: close\r\n"
             << extraHeaders
             << "\r\n";

    if (method != "HEAD")
        response << responseBody;

    sendAll (clientSocket, response.str());

    shutdown (clientSocket, SD_SEND);
    closesocket (clientSocket);
}

void WebServer::run()
{
    while (running_.load() && serverSocket_ != INVALID_SOCKET)
    {
        sockaddr_in clientAddr {};
        int clientAddrLen = sizeof (clientAddr);

        const auto clientSocket = accept (serverSocket_, (sockaddr*) &clientAddr, &clientAddrLen);

        if (clientSocket == INVALID_SOCKET)
        {
            const auto error = WSAGetLastError();

            if (error == WSAEWOULDBLOCK)
            {
                std::this_thread::sleep_for (std::chrono::milliseconds (5));
                continue;
            }

            if (error == WSAEINTR || error == WSAENOTSOCK || ! running_.load())
                break;

            std::this_thread::sleep_for (std::chrono::milliseconds (50));
            continue;
        }

        if (! running_.load())
        {
            closesocket (clientSocket);
            break;
        }

        // Counted before the thread starts, so stop() can never observe zero
        // while a handler is about to begin.
        activeConnections_.fetch_add (1);

        std::thread ([this, clientSocket]
        {
            handleConnection (clientSocket);
            activeConnections_.fetch_sub (1);
        }).detach();
    }
}
