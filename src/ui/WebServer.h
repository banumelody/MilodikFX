#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

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

    /**
     * Turns one path into a Server-Sent Events stream.
     *
     * The connection is held open and `source` is called every `intervalMs` to
     * produce the next event body. This is a different connection model from
     * every other path here -- a stream occupies its thread for as long as the
     * page is open -- so the number of them is capped, and `source` is called
     * from the connection thread and must not block.
     *
     * Must be called before start(); the thread reads these without a lock.
     */
    void registerEventStream (std::string path, std::function<std::string()> source, int intervalMs);

private:
    // Anything larger than this is not a control message we asked for.
    static constexpr int kMaxRequestBodyBytes = 1 * 1024 * 1024;
    static constexpr int kMaxHeaderBytes = 32 * 1024;

    /**
     * How many streams may be open at once.
     *
     * Each one holds a thread until the page closes it. A reloading browser can
     * briefly leave the old connection open alongside the new, so this needs
     * headroom above the one stream the UI actually uses -- but it must stay
     * bounded, or a page in a reload loop would spawn threads without limit.
     */
    static constexpr int kMaxEventStreams = 4;

    struct EventStream
    {
        std::string path;
        std::function<std::string()> source;
        int intervalMs = 33;
    };

    int port_;
    std::atomic<bool> running_ { false };
    std::atomic<int> activeConnections_ { 0 };
    std::atomic<int> activeStreams_ { 0 };
    SOCKET serverSocket_ = INVALID_SOCKET;
    RestApiDispatcher dispatcher_;
    std::vector<EventStream> eventStreams_;

    void run() override;

    std::string getMimeType (const std::string& filePath) const;
    juce::File getWebRoot() const;
    std::string buildFallbackPage() const;

    void handleConnection (SOCKET clientSocket);
    std::string parseHttpRequest (const std::string& request, std::string& path, std::string& method) const;

    const EventStream* findEventStream (const std::string& path) const;
    void runEventStream (SOCKET clientSocket, const EventStream& stream);

    static bool setSocketNonBlocking (SOCKET sock, bool nonBlocking);
    static void sendAll (SOCKET sock, const std::string& payload);

    /** Same as sendAll but reports whether the peer is still there. */
    static bool sendAllChecked (SOCKET sock, const std::string& payload);
};
