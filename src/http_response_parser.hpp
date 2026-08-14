#pragma once

#include <string>

struct ParsedHttpResponse {
    int status_code = 0;
    std::string body;
    std::string error;

    bool ok() const { return ((status_code >= 200 && status_code < 300) || status_code == 304) && error.empty(); }
};

ParsedHttpResponse parseHttpResponse(const std::string& raw);
bool httpResponseComplete(const std::string& raw, const std::string& method = {});
