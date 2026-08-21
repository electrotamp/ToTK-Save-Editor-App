#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace totk::net {

struct HttpResponse {
    bool success = false;
    long statusCode = 0;
    std::vector<uint8_t> body;
    std::string error;
};

// Thin libcurl wrapper (switch-curl + switch-mbedtls portlibs). Every call
// here is blocking network I/O — callers must run it off the UI thread via
// brls::async and marshal results back through brls::sync, same as the rest
// of the app's async work. Not thread-safe to call concurrently from multiple
// threads.
class HttpClient {
public:
    // Brings up libnx sockets + libcurl global state. Call once during app
    // startup before any get() call; safe to call more than once.
    static void initialize();
    static void shutdown();

    static HttpResponse get(const std::string& url);

    // Fetches every URL concurrently on a shared libcurl multi-handle (real
    // network-level concurrency via non-blocking sockets, no extra OS
    // threads — see http_client.cpp) instead of one full connection+TLS
    // handshake at a time. Still a single blocking call from the caller's
    // point of view — run it via brls::async like get(). Empty strings in
    // `urls` are skipped (their response is left unsuccessful) so callers
    // can pass a sparse list without pre-filtering. Results are returned in
    // the same order as `urls`.
    static std::vector<HttpResponse> getMany(const std::vector<std::string>& urls);
};

}  // namespace totk::net
