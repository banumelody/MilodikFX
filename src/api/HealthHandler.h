#pragma once

#include "HttpHandler.h"
#include <string>

// Simple health check handler used by CI and readiness probes.
class HealthHandler : public HttpHandler
{
public:
    HealthHandler() = default;
    ~HealthHandler() override = default;

    Response handleGet(const std::string& /*path*/, const std::string& /*query*/) const override
    {
        return { 200, "application/json", R"({"status":"ok"})" };
    }
};
