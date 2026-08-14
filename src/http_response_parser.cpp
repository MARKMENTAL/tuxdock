#include "http_response_parser.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool decodeChunked(const std::string& input, std::string& body, std::string& error) {
    std::size_t offset = 0;
    while (offset < input.size()) {
        const auto line_end = input.find("\r\n", offset);
        if (line_end == std::string::npos) {
            error = "Truncated chunk size.";
            return false;
        }
        const std::string size_text = trim(input.substr(offset, line_end - offset));
        const auto semicolon = size_text.find(';');
        const std::string size_value = size_text.substr(0, semicolon);
        std::size_t chunk_size = 0;
        try {
            chunk_size = std::stoull(size_value, nullptr, 16);
        } catch (...) {
            error = "Invalid chunk size.";
            return false;
        }
        offset = line_end + 2;
        if (chunk_size == 0) return true;
        if (offset + chunk_size + 2 > input.size() || input.substr(offset + chunk_size, 2) != "\r\n") {
            error = "Truncated chunk data.";
            return false;
        }
        body.append(input, offset, chunk_size);
        offset += chunk_size + 2;
    }
    error = "Missing terminating chunk.";
    return false;
}

}  // namespace

ParsedHttpResponse parseHttpResponse(const std::string& raw) {
    ParsedHttpResponse response;
    const auto header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        response.error = "Invalid response from Docker Engine: missing headers.";
        return response;
    }

    const auto status_end = raw.find("\r\n");
    std::istringstream status_line(raw.substr(0, status_end));
    std::string version;
    status_line >> version >> response.status_code;
    if (response.status_code == 0) {
        response.error = "Invalid HTTP status from Docker Engine.";
        return response;
    }

    const bool bodyless = (response.status_code >= 100 && response.status_code < 200) ||
                          response.status_code == 204 || response.status_code == 304;

    std::size_t content_length = std::string::npos;
    bool chunked = false;
    std::size_t line_start = status_end + 2;
    while (line_start < header_end) {
        const auto line_end = raw.find("\r\n", line_start);
        if (line_end == std::string::npos || line_end > header_end) break;
        const auto separator = raw.find(':', line_start);
        if (separator != std::string::npos && separator < line_end) {
            const auto name = lower(trim(raw.substr(line_start, separator - line_start)));
            const auto value = lower(trim(raw.substr(separator + 1, line_end - separator - 1)));
            if (name == "content-length") {
                try { content_length = std::stoull(value); } catch (...) { response.error = "Invalid Content-Length."; return response; }
            } else if (name == "transfer-encoding" && value.find("chunked") != std::string::npos) {
                chunked = true;
            }
        }
        line_start = line_end + 2;
    }

    const std::string payload = raw.substr(header_end + 4);
    if (bodyless) {
        response.body.clear();
    } else if (chunked) {
        if (!decodeChunked(payload, response.body, response.error)) return response;
    } else if (content_length != std::string::npos) {
        if (payload.size() < content_length) {
            response.error = "Truncated HTTP response body.";
            return response;
        }
        response.body = payload.substr(0, content_length);
    } else {
        response.body = payload;
    }
    if (response.status_code < 200 || response.status_code >= 300) {
        if (response.status_code == 304) return response;
        response.error = response.body.empty() ? "Docker Engine request failed." : response.body;
    }
    return response;
}

bool httpResponseComplete(const std::string& raw, const std::string& method) {
    const auto header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos) return false;
    const auto status_end = raw.find("\r\n");
    if (status_end == std::string::npos || status_end > header_end) return false;
    std::istringstream status_line(raw.substr(0, status_end));
    std::string version;
    int status_code = 0;
    status_line >> version >> status_code;
    if (method == "HEAD" || (status_code >= 100 && status_code < 200) || status_code == 204 || status_code == 304) return true;
    const std::string headers = raw.substr(status_end + 2, header_end - status_end - 2);
    const auto transfer = headers.find("Transfer-Encoding:");
    if (transfer != std::string::npos && lower(headers.substr(transfer)).find("chunked") != std::string::npos) {
        return raw.size() >= header_end + 7 && raw.find("\r\n0\r\n", header_end + 4) != std::string::npos;
    }
    const auto length = lower(headers).find("content-length:");
    if (length == std::string::npos) return false;
    const auto line_end = headers.find("\r\n", length);
    const auto value_start = headers.find(':', length);
    if (value_start == std::string::npos) return false;
    try {
        const auto expected = std::stoull(trim(headers.substr(value_start + 1, line_end - value_start - 1)));
        return raw.size() >= header_end + 4 + expected;
    } catch (...) {
        return false;
    }
}
