#pragma once

#include "HttpHandler.h"
#include <map>
#include <memory>
#include <string>
#include <functional>

/**
 * REST API dispatcher that routes HTTP requests to handlers.
 * Matches paths like /api/devices, /api/parameters/*, etc.
 */
class RestApiDispatcher
{
public:
    RestApiDispatcher() = default;
    ~RestApiDispatcher() = default;

    // Prevent copying
    RestApiDispatcher(const RestApiDispatcher&) = delete;
    RestApiDispatcher& operator=(const RestApiDispatcher&) = delete;

    /**
     * Register a handler for a path prefix
     * @param pathPrefix e.g., "/api/devices", "/api/parameters"
     * @param handler The handler to invoke for this path
     */
    void registerHandler(const std::string& pathPrefix, std::shared_ptr<HttpHandler> handler)
    {
        handlers_[pathPrefix] = handler;
    }

    /**
     * Dispatch an HTTP request to the appropriate handler
     * @param method HTTP method (GET, POST, PUT, DELETE)
     * @param path The request path
     * @param query Query string
     * @param body Request body (for POST/PUT)
     * @return HTTP response
     */
    HttpHandler::Response dispatch(
        const std::string& method,
        const std::string& path,
        const std::string& query = "",
        const std::string& body = "")
    {
        // Find the best matching handler (longest prefix match)
        std::shared_ptr<HttpHandler> bestHandler;
        std::string bestPath;

        for (const auto& [prefix, handler] : handlers_)
        {
            if (path.find(prefix) == 0) // Path starts with prefix
            {
                if (prefix.length() > bestPath.length())
                {
                    bestHandler = handler;
                    bestPath = prefix;
                }
            }
        }

        if (!bestHandler)
        {
            return {
                404,
                "application/json",
                R"({"error":"Endpoint not found"})"
            };
        }

        // Dispatch to appropriate HTTP method
        if (method == "GET")
            return bestHandler->handleGet(path, query);
        else if (method == "POST")
            return bestHandler->handlePost(path, body);
        else if (method == "PUT")
            return bestHandler->handlePut(path, body);
        else if (method == "DELETE")
            return bestHandler->handleDelete(path);
        else
            return {
                405,
                "application/json",
                R"({"error":"Method not allowed"})"
            };
    }

private:
    std::map<std::string, std::shared_ptr<HttpHandler>> handlers_;
};
