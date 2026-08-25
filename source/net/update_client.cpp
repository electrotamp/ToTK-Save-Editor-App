#include "net/update_client.hpp"

#include "net/http_client.hpp"
#include "util/file_text.hpp"
#include "util/totk_log.hpp"

#include <borealis.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>

#if defined(__SWITCH__)
#include <switch.h>
#endif

#ifndef APP_VERSION_STRING
#define APP_VERSION_STRING "0.0.0"
#endif

namespace totk::net {

namespace {

// Real published releases (confirmed 2026-08-25) — see updater_feature_research
// project memory for why this isn't the repo the local git `origin` remote
// points to.
constexpr const char* kReleasesApiUrl =
    "https://api.github.com/repos/electrotamp/ToTK-Save-Editor-App/releases/latest";
constexpr const char* kAssetName = "totk_save_editor.nro";
constexpr const char* kInstallPath = "sdmc:/switch/totk-save-editor/totk_save_editor.nro";
constexpr const char* kTempPath = "sdmc:/switch/totk-save-editor/totk_save_editor.nro.update";
constexpr const char* kSettingsPath = "sdmc:/switch/totk-save-editor/update_settings.json";

std::vector<int> parseVersionParts(const std::string& version) {
    std::vector<int> parts;
    size_t start = 0;
    while (start <= version.size()) {
        const size_t dot = version.find('.', start);
        const std::string part = version.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        int value = 0;
        for (char c : part) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                value = 0;
                break;
            }
            value = value * 10 + (c - '0');
        }
        parts.push_back(value);
        if (dot == std::string::npos) break;
        start = dot + 1;
    }
    return parts;
}

std::string stripLeadingV(const std::string& tag) {
    if (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V')) return tag.substr(1);
    return tag;
}

}  // namespace

std::string currentVersionString() { return APP_VERSION_STRING; }

bool isNewerVersion(const std::string& latest, const std::string& current) {
    const std::vector<int> latestParts = parseVersionParts(latest);
    const std::vector<int> currentParts = parseVersionParts(current);
    const size_t count = std::max(latestParts.size(), currentParts.size());
    for (size_t i = 0; i < count; ++i) {
        const int latestValue = i < latestParts.size() ? latestParts[i] : 0;
        const int currentValue = i < currentParts.size() ? currentParts[i] : 0;
        if (latestValue != currentValue) return latestValue > currentValue;
    }
    return false;
}

bool fetchLatestRelease(UpdateInfo& outInfo, std::string& error) {
    const HttpResponse response = HttpClient::get(kReleasesApiUrl);
    if (!response.success) {
        error = response.error.empty() ? "Failed to reach GitHub." : response.error;
        return false;
    }

    const std::string text(response.body.begin(), response.body.end());
    const nlohmann::json json = nlohmann::json::parse(text, nullptr, false);
    if (json.is_discarded() || !json.is_object()) {
        error = "Could not read release info (unexpected response).";
        return false;
    }

    outInfo.version = stripLeadingV(json.value("tag_name", ""));
    outInfo.releaseUrl = json.value("html_url", "");
    if (outInfo.version.empty()) {
        error = "Release response had no version tag.";
        return false;
    }

    for (const auto& asset : json.value("assets", nlohmann::json::array())) {
        if (asset.value("name", "") == kAssetName) {
            outInfo.downloadUrl = asset.value("browser_download_url", "");
            break;
        }
    }
    if (outInfo.downloadUrl.empty()) {
        error = "Latest release has no " + std::string(kAssetName) + " asset.";
        return false;
    }

    error.clear();
    return true;
}

bool downloadAndInstallUpdate(const UpdateInfo& info, std::string& error) {
#if !defined(__SWITCH__)
    (void)info;
    error = "Updates can only be installed on Nintendo Switch.";
    return false;
#else
    if (info.downloadUrl.empty()) {
        error = "No download URL.";
        return false;
    }

    const HttpResponse response = HttpClient::get(info.downloadUrl);
    if (!response.success) {
        error = response.error.empty() ? "Download failed." : response.error;
        return false;
    }
    if (response.body.empty()) {
        error = "Downloaded file was empty.";
        return false;
    }

    if (!totk::ensureParentDirectory(kTempPath)) {
        error = "Could not create install directory on SD card.";
        return false;
    }

    FILE* file = fopen(kTempPath, "wb");
    if (!file) {
        error = "Could not open a temp file for writing on the SD card.";
        return false;
    }
    const size_t written = fwrite(response.body.data(), 1, response.body.size(), file);
    fclose(file);
    if (written != response.body.size()) {
        remove(kTempPath);
        error = "Failed to write the update to the SD card.";
        return false;
    }

    // Drop any existing file first — rename() over an existing destination
    // isn't guaranteed to succeed on every filesystem devkitPro's fs driver
    // sits on top of.
    remove(kInstallPath);
    if (rename(kTempPath, kInstallPath) != 0) {
        error = "Failed to move the update into place.";
        return false;
    }

    TOTK_LOG("updater: installed version=%s at %s", info.version.c_str(), kInstallPath);
    error.clear();
    return true;
#endif
}

bool relaunchIntoUpdatedApp() {
#if !defined(__SWITCH__)
    return false;
#else
    if (!envHasNextLoad()) {
        TOTK_LOG("updater: envHasNextLoad()=false, cannot auto-relaunch");
        return false;
    }
    const Result rc = envSetNextLoad(kInstallPath, "");
    if (R_FAILED(rc)) {
        TOTK_LOG("updater: envSetNextLoad failed rc=0x%x", rc);
        return false;
    }
    TOTK_LOG("updater: relaunching into updated build");
    brls::Application::quit();
    return true;
#endif
}

bool autoCheckEnabled() {
#if !defined(__SWITCH__)
    return false;
#else
    const std::string text = totk::loadFileText(kSettingsPath);
    if (text.empty()) return true;
    const nlohmann::json json = nlohmann::json::parse(text, nullptr, false);
    if (json.is_discarded() || !json.is_object()) return true;
    return json.value("auto_check_enabled", true);
#endif
}

void setAutoCheckEnabled(bool enabled) {
#if defined(__SWITCH__)
    nlohmann::json json;
    json["auto_check_enabled"] = enabled;
    totk::writeFileText(kSettingsPath, json.dump(2));
#else
    (void)enabled;
#endif
}

}  // namespace totk::net
