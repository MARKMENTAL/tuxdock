#include "http_response_parser.hpp"

#include <cassert>
#include <iostream>

void testContentLength() {
    const auto response = parseHttpResponse(
        "HTTP/1.1 200 OK\r\nContent-Length: 6\r\n\r\n[1,2]\n");
    if (!response.ok()) std::cerr << response.error << " status=" << response.status_code << " body=" << response.body << "\n";
    assert(response.ok());
    assert(response.body == "[1,2]\n");
}

void testChunked() {
    const auto response = parseHttpResponse(
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
        "5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n");
    assert(response.ok());
    assert(response.body == "hello world");
}

void testTruncatedBody() {
    const auto response = parseHttpResponse(
        "HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nshort");
    assert(!response.ok());
    assert(response.error == "Truncated HTTP response body.");
}

void testErrorResponse() {
    const auto response = parseHttpResponse(
        "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 3\r\n\r\nbad");
    assert(!response.ok());
    assert(response.status_code == 500);
    assert(response.error == "bad");
}

void testCompleteResponseDoesNotNeedPeerClose() {
    const std::string raw = "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\n\r\n";
    assert(httpResponseComplete(raw));
    const auto response = parseHttpResponse(raw);
    assert(response.ok());
    assert(response.status_code == 204);
}

void testBodylessStatuses() {
    for (const int status : {204, 304}) {
        const std::string raw = "HTTP/1.1 " + std::to_string(status) + " Status\r\n\r\n";
        assert(httpResponseComplete(raw));
        const auto parsed = parseHttpResponse(raw);
        if (!parsed.ok()) std::cerr << "status=" << status << " error=" << parsed.error << "\n";
        assert(parsed.ok());
    }
    const std::string head = "HTTP/1.1 200 OK\r\n\r\n";
    assert(httpResponseComplete(head, "HEAD"));
}

int main() {
    testContentLength();
    testChunked();
    testTruncatedBody();
    testErrorResponse();
    testCompleteResponseDoesNotNeedPeerClose();
    testBodylessStatuses();
    std::cout << "HTTP response parser tests passed\n";
}
