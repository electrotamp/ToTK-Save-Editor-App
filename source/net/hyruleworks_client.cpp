#include "net/hyruleworks_client.hpp"

#include "net/http_client.hpp"
#include "util/totk_log.hpp"

#include <cctype>

namespace totk::net {

namespace {

constexpr const char* kBaseUrl = "https://www.hyruleworks.com";
// The site's own public image and blueprint-file CloudFront distributions —
// both confirmed live 2026-08-17 by downloading a real build's <img srcSet>
// and "Download (.cai)" href straight from its detail page. The raw S3
// buckets behind them (hyruleworks.s3.*.amazonaws.com and
// hyruleworks-files.s3.*.amazonaws.com) return 403 AccessDenied directly —
// only these CDN hosts are actually public.
constexpr const char* kImageCdnHost = "https://d1r4ymb1he24h1.cloudfront.net";
constexpr const char* kBlueprintCdnHost = "https://d2fvvq21pbg82w.cloudfront.net";
constexpr const char* kImageBucketMarker = "hyruleworks.s3.us-east-2.amazonaws.com";
constexpr const char* kBlueprintBucketMarker = "hyruleworks-files.s3.us-east-2.amazonaws.com";

// The listing/detail pages are server-rendered Next.js apps that embed their
// data as an escaped JSON blob inside a streamed RSC payload (`self.__next_f
// .push(...)`), not as plain HTML card markup — there's no clean DOM to
// select against. Every field of interest still appears as a literal
// `\"key\":\"value\"` (or `\"key\":123` / `\"key\":[...]`) run in the raw
// response bytes, confirmed by hex-inspecting real responses on 2026-08-17,
// so a small purpose-built scanner over that escaped form is more robust
// here than trying to walk the surrounding HTML/JS as a tree. If HyruleWorks
// changes its data-embedding shape this whole file needs revisiting — kept
// isolated here for exactly that reason.

// Finds `\"<key>\":\"` and returns the decoded value up to the next
// unescaped `\"` terminator. `\n`/`\t`/`\\`/`\/` are unescaped; any other
// `\X` sequence is passed through as literal X (best-effort — good enough for
// titles/descriptions/usernames, which is all this is used for).
bool extractStringField(const std::string& text, const std::string& key, size_t searchFrom, std::string& outValue,
                         size_t& outAfterPos) {
    const std::string token = "\\\"" + key + "\\\":\\\"";
    const size_t pos = text.find(token, searchFrom);
    if (pos == std::string::npos) return false;

    std::string value;
    size_t i = pos + token.size();
    while (i < text.size()) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            const char next = text[i + 1];
            if (next == '"') {
                i += 2;
                outValue = value;
                outAfterPos = i;
                return true;
            }
            switch (next) {
                case 'n': value.push_back('\n'); break;
                case 't': value.push_back('\t'); break;
                case '\\': value.push_back('\\'); break;
                case '/': value.push_back('/'); break;
                default: value.push_back(next); break;
            }
            i += 2;
            continue;
        }
        value.push_back(text[i]);
        ++i;
    }
    return false;  // ran off the end without a terminator — malformed/truncated
}

bool extractIntField(const std::string& text, const std::string& key, size_t searchFrom, int& outValue,
                      size_t& outAfterPos) {
    const std::string token = "\\\"" + key + "\\\":";
    const size_t pos = text.find(token, searchFrom);
    if (pos == std::string::npos) return false;

    size_t i = pos + token.size();
    bool negative = false;
    if (i < text.size() && text[i] == '-') {
        negative = true;
        ++i;
    }
    const size_t digitsStart = i;
    long value = 0;
    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
        value = value * 10 + (text[i] - '0');
        ++i;
    }
    if (i == digitsStart) return false;

    outValue = static_cast<int>(negative ? -value : value);
    outAfterPos = i;
    return true;
}

// Extracts a `\"key\":[\"a\",\"b\"]` string array. Values are assumed not to
// contain `]` (true for the URL/tag-name arrays this is used on).
bool extractStringArrayField(const std::string& text, const std::string& key, size_t searchFrom,
                              std::vector<std::string>& outValues, size_t& outAfterPos) {
    const std::string token = "\\\"" + key + "\\\":[";
    const size_t pos = text.find(token, searchFrom);
    if (pos == std::string::npos) return false;

    const size_t spanStart = pos + token.size();
    const size_t spanEnd = text.find(']', spanStart);
    if (spanEnd == std::string::npos) return false;

    size_t i = spanStart;
    while (i < spanEnd) {
        const size_t quoteStart = text.find("\\\"", i);
        if (quoteStart == std::string::npos || quoteStart >= spanEnd) break;
        const size_t quoteEnd = text.find("\\\"", quoteStart + 2);
        if (quoteEnd == std::string::npos || quoteEnd > spanEnd) break;
        outValues.push_back(text.substr(quoteStart + 2, quoteEnd - (quoteStart + 2)));
        i = quoteEnd + 2;
    }

    outAfterPos = spanEnd + 1;
    return true;
}

// Bounds how far a per-card field search is allowed to look ahead of the
// card's `\"creator\":{` anchor, so a field missing on one card can't
// accidentally pick up the next card's value.
constexpr size_t kCardFieldWindow = 3000;

void parseCatalogCards(const std::string& text, std::vector<HyruleWorksBuildSummary>& outBuilds) {
    size_t searchPos = 0;
    while (true) {
        const size_t creatorTag = text.find("\\\"creator\\\":{", searchPos);
        if (creatorTag == std::string::npos) break;

        // The card's own id sits immediately before its creator object:
        // `\"id\":379,\"creator\":{...`. rfind bounded to a small lookback
        // window so we don't grab a stray "id" from an unrelated object.
        const size_t lookbackStart = creatorTag > 64 ? creatorTag - 64 : 0;
        const size_t idKeyPos = text.rfind("\\\"id\\\":", creatorTag);
        searchPos = creatorTag + 1;
        if (idKeyPos == std::string::npos || idKeyPos < lookbackStart) continue;

        int id = 0;
        size_t cursor = 0;
        if (!extractIntField(text, "id", idKeyPos, id, cursor)) continue;

        const size_t windowEnd = std::min(text.size(), creatorTag + kCardFieldWindow);
        const std::string window = text.substr(creatorTag, windowEnd - creatorTag);

        HyruleWorksBuildSummary build;
        build.id = id;
        size_t afterPos = 0;
        extractStringField(window, "username", 0, build.creator, afterPos);
        extractStringField(window, "name", 0, build.name, afterPos);
        extractStringField(window, "description", 0, build.description, afterPos);
        extractStringField(window, "imageUrl", 0, build.imageUrl, afterPos);
        extractStringField(window, "caiFileUrl", 0, build.caiFileUrl, afterPos);

        if (!build.name.empty()) outBuilds.push_back(std::move(build));
    }
}

bool parseCatalogHasNextPage(const std::string& text, size_t parsedBuildCount) {
    // HyruleWorks renders the pager as a plain HTML button. On the terminal
    // page its Next button is still present but carries hidden="". Read that
    // authoritative marker instead of guessing from the number of builds
    // that remain after the app filters out entries without blueprints.
    const size_t nextText = text.find(">Next<span");
    if (nextText != std::string::npos) {
        const size_t buttonStart = text.rfind("<button", nextText);
        const size_t buttonEnd = buttonStart == std::string::npos ? std::string::npos : text.find('>', buttonStart);
        if (buttonStart != std::string::npos && buttonEnd != std::string::npos && buttonEnd < nextText) {
            const std::string buttonTag = text.substr(buttonStart, buttonEnd - buttonStart + 1);
            return buttonTag.find("hidden") == std::string::npos &&
                   buttonTag.find("disabled") == std::string::npos &&
                   buttonTag.find("aria-disabled=\"true\"") == std::string::npos;
        }
    }

    // Layout-change fallback: the currently documented site page size is 30.
    return parsedBuildCount >= 30;
}

std::string urlEncode(const std::string& value) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size());
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0x0F]);
        }
    }
    return out;
}

}  // namespace

std::string hyruleWorksImageCdnUrl(const std::string& rawImageUrl, int width) {
    const size_t markerPos = rawImageUrl.find(kImageBucketMarker);
    if (markerPos == std::string::npos) return rawImageUrl;  // already a CDN/unexpected URL — pass through

    const size_t pathStart = markerPos + std::string(kImageBucketMarker).size();
    const std::string path = rawImageUrl.substr(pathStart);
    return std::string(kImageCdnHost) + path + "?format=jpeg&width=" + std::to_string(width) + "&quality=80";
}

namespace {

std::string blueprintCdnUrl(const std::string& rawCaiUrl) {
    const size_t markerPos = rawCaiUrl.find(kBlueprintBucketMarker);
    if (markerPos == std::string::npos) return rawCaiUrl;
    const size_t pathStart = markerPos + std::string(kBlueprintBucketMarker).size();
    return std::string(kBlueprintCdnHost) + rawCaiUrl.substr(pathStart);
}

}  // namespace

bool fetchHyruleWorksCatalogPage(int page, const std::string& searchQuery,
                                  std::vector<HyruleWorksBuildSummary>& outBuilds, bool& outHasNextPage,
                                  std::string& error) {
    outHasNextPage = false;
    std::string url = std::string(kBaseUrl) + "/?page=" + std::to_string(page);
    if (!searchQuery.empty()) url += "&name=" + urlEncode(searchQuery);

    const HttpResponse response = HttpClient::get(url);
    if (!response.success) {
        error = response.error.empty() ? "Failed to reach HyruleWorks." : response.error;
        return false;
    }

    const std::string text(response.body.begin(), response.body.end());
    parseCatalogCards(text, outBuilds);
    outHasNextPage = parseCatalogHasNextPage(text, outBuilds.size());
    if (outBuilds.empty()) {
        error = "No builds found.";
    }
    return true;
}

bool fetchHyruleWorksBuildDetail(int buildId, HyruleWorksBuildDetail& outDetail, std::string& error) {
    const std::string url = std::string(kBaseUrl) + "/builds/" + std::to_string(buildId);
    const HttpResponse response = HttpClient::get(url);
    if (!response.success) {
        error = response.error.empty() ? "Failed to reach HyruleWorks." : response.error;
        return false;
    }

    const std::string text(response.body.begin(), response.body.end());
    // Anchor on the build's own `\"creator\":{...}` object, same as the
    // catalog cards — unlike "name"/"description", which also appear as
    // unrelated `<meta name="...">` JSON well before the real build data,
    // this occurs exactly once per detail page (confirmed live 2026-08-17).
    const size_t creatorTag = text.find("\\\"creator\\\":{");
    if (creatorTag == std::string::npos) {
        error = "Could not read build details (page layout may have changed).";
        return false;
    }

    outDetail.id = buildId;
    size_t afterPos = 0;
    extractStringField(text, "username", creatorTag, outDetail.creator, afterPos);
    extractStringField(text, "name", creatorTag, outDetail.name, afterPos);
    extractStringField(text, "description", creatorTag, outDetail.description, afterPos);
    extractStringArrayField(text, "imageUrls", creatorTag, outDetail.imageUrls, afterPos);
    extractStringField(text, "caiFileUrl", creatorTag, outDetail.caiFileUrl, afterPos);
    extractStringArrayField(text, "tagNames", creatorTag, outDetail.tags, afterPos);
    if (!outDetail.imageUrls.empty()) outDetail.imageUrl = outDetail.imageUrls.front();

    if (outDetail.name.empty()) {
        error = "Could not read build details (page layout may have changed).";
        return false;
    }
    error.clear();
    return true;
}

bool downloadHyruleWorksBlueprint(const std::string& caiFileUrl, std::vector<uint8_t>& outBytes, std::string& error) {
    if (caiFileUrl.empty()) {
        error = "This build has no downloadable blueprint.";
        return false;
    }

    const HttpResponse response = HttpClient::get(blueprintCdnUrl(caiFileUrl));
    if (!response.success) {
        error = response.error.empty() ? "Failed to download blueprint." : response.error;
        return false;
    }

    outBytes = response.body;
    error.clear();
    return true;
}

}  // namespace totk::net
