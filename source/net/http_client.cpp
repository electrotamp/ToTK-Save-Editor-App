#include "net/http_client.hpp"

#include "util/totk_log.hpp"

#ifndef __SWITCH__

namespace totk::net {

void HttpClient::initialize() {}
void HttpClient::shutdown() {}

HttpResponse HttpClient::get(const std::string& url) {
    (void)url;
    HttpResponse response;
    response.error = "Networking is only available on Nintendo Switch.";
    return response;
}

std::vector<HttpResponse> HttpClient::getMany(const std::vector<std::string>& urls) {
    std::vector<HttpResponse> responses(urls.size());
    for (auto& r : responses) r.error = "Networking is only available on Nintendo Switch.";
    return responses;
}

}  // namespace totk::net

#else

#include <curl/curl.h>
#include <switch.h>

namespace totk::net {

namespace {

bool gInitialized = false;

size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* body = static_cast<std::vector<uint8_t>*>(userdata);
    const size_t bytes = size * nmemb;
    body->insert(body->end(), ptr, ptr + bytes);
    return bytes;
}

// Shared option set between get() and getMany() — every easy handle this
// client creates is configured identically.
CURL* makeEasyHandle(const std::string& url, std::vector<uint8_t>* body) {
    CURL* curl = curl_easy_init();
    if (!curl) return nullptr;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "totk-save-editor/1.0 (+homebrew)");
    // Bundled Mozilla CA set (resources/data/cacert.pem -> romfs:/data/cacert.pem)
    // — devkitPro's switch-curl/mbedtls ship no default trust store, and
    // disabling verification (as some homebrew networking code does) trades
    // away a real security property for no reason when bundling a cert file
    // is this easy.
    curl_easy_setopt(curl, CURLOPT_CAINFO, "romfs:/data/cacert.pem");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    return curl;
}

}  // namespace

void HttpClient::initialize() {
    if (gInitialized) return;

    // Sockets are already brought up by switch_wrapper.c's userAppInit()
    // (socketInitialize(), unconditional, every build) before main() even
    // runs — calling socketInitializeDefault() again here always failed with
    // LibnxError_AlreadyInitialized (0xf59), which this code treated as
    // fatal and permanently left networking disabled. Socket lifecycle is
    // userAppInit()/userAppExit()'s to own; just use it.
    curl_global_init(CURL_GLOBAL_DEFAULT);
    gInitialized = true;
}

void HttpClient::shutdown() {
    if (!gInitialized) return;
    curl_global_cleanup();
    gInitialized = false;
}

HttpResponse HttpClient::get(const std::string& url) {
    HttpResponse response;
    if (!gInitialized) {
        response.error = "Networking not initialized.";
        return response;
    }

    CURL* curl = makeEasyHandle(url, &response.body);
    if (!curl) {
        response.error = "curl_easy_init failed.";
        return response;
    }

    const CURLcode result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        response.error = curl_easy_strerror(result);
        curl_easy_cleanup(curl);
        return response;
    }

    long statusCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
    curl_easy_cleanup(curl);

    response.statusCode = statusCode;
    response.success = statusCode >= 200 && statusCode < 300;
    if (!response.success) {
        response.error = "HTTP " + std::to_string(statusCode);
    }
    return response;
}

std::vector<HttpResponse> HttpClient::getMany(const std::vector<std::string>& urls) {
    std::vector<HttpResponse> responses(urls.size());
    if (!gInitialized || urls.empty()) {
        for (auto& r : responses) r.error = "Networking not initialized.";
        return responses;
    }

    CURLM* multi = curl_multi_init();
    if (!multi) {
        for (auto& r : responses) r.error = "curl_multi_init failed.";
        return responses;
    }

    // Cap concurrent connections rather than opening every socket at once —
    // this console is memory-constrained and this app already had one real
    // crash from an unrelated burst-allocation issue the same day this was
    // written (see workflow_testing project memory); no reason to also open
    // dozens of simultaneous TLS connections for a thumbnail batch.
    constexpr long kMaxConcurrent = 6;
    curl_multi_setopt(multi, CURLMOPT_MAX_TOTAL_CONNECTIONS, kMaxConcurrent);

    std::vector<CURL*> handles(urls.size(), nullptr);
    for (size_t i = 0; i < urls.size(); ++i) {
        if (urls[i].empty()) {
            responses[i].error = "Empty URL.";
            continue;
        }
        handles[i] = makeEasyHandle(urls[i], &responses[i].body);
        if (handles[i]) {
            curl_multi_add_handle(multi, handles[i]);
        } else {
            responses[i].error = "curl_easy_init failed.";
        }
    }

    int stillRunning = 0;
    curl_multi_perform(multi, &stillRunning);
    while (stillRunning > 0) {
        int numfds = 0;
        curl_multi_wait(multi, nullptr, 0, 1000, &numfds);
        curl_multi_perform(multi, &stillRunning);
    }

    int messagesLeft = 0;
    CURLMsg* msg = nullptr;
    while ((msg = curl_multi_info_read(multi, &messagesLeft)) != nullptr) {
        if (msg->msg != CURLMSG_DONE) continue;
        CURL* handle = msg->easy_handle;
        size_t index = handles.size();
        for (size_t i = 0; i < handles.size(); ++i) {
            if (handles[i] == handle) {
                index = i;
                break;
            }
        }
        if (index == handles.size()) continue;

        HttpResponse& response = responses[index];
        if (msg->data.result != CURLE_OK) {
            response.error = curl_easy_strerror(msg->data.result);
            continue;
        }
        long statusCode = 0;
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &statusCode);
        response.statusCode = statusCode;
        response.success = statusCode >= 200 && statusCode < 300;
        if (!response.success) response.error = "HTTP " + std::to_string(statusCode);
    }

    for (CURL* handle : handles) {
        if (!handle) continue;
        curl_multi_remove_handle(multi, handle);
        curl_easy_cleanup(handle);
    }
    curl_multi_cleanup(multi);

    return responses;
}

}  // namespace totk::net

#endif
