#pragma once

#include <string>
#include <memory>

/**
 * Base class for REST API endpoint handlers.
 * Subclasses implement GET, PUT, POST, DELETE operations.
 */
class HttpHandler
{
public:
    virtual ~HttpHandler() = default;

    struct Response
    {
        int statusCode = 200;
        std::string contentType = "application/json";
        std::string body;
    };

    /**
     * Handle GET request
     * @param path The request path (e.g., "/api/devices" or "/api/parameters/master-volume")
     * @param query Query string parameters (e.g., "device=input")
     */
    virtual Response handleGet(const std::string& path, const std::string& query) const
    {
        return { 405, "application/json", R"({"error":"Method Not Allowed"})" };
    }

    /**
     * Handle POST request
     * @param path The request path
     * @param body The request body (JSON)
     */
    virtual Response handlePost(const std::string& path, const std::string& body)
    {
        return { 405, "application/json", R"({"error":"Method Not Allowed"})" };
    }

    /**
     * Handle PUT request
     * @param path The request path
     * @param body The request body (JSON)
     */
    virtual Response handlePut(const std::string& path, const std::string& body)
    {
        return { 405, "application/json", R"({"error":"Method Not Allowed"})" };
    }

    /**
     * Handle DELETE request
     * @param path The request path
     */
    virtual Response handleDelete(const std::string& path)
    {
        return { 405, "application/json", R"({"error":"Method Not Allowed"})" };
    }
};
