#include "ui/autobuild_icon_store.hpp"

#include <cstdio>
#include <iomanip>
#include <sstream>

#include "util/file_text.hpp"

namespace totk::ui {

namespace {
constexpr const char* kIconDirectory = "sdmc:/switch/totk-save-editor/autobuild_icons";
}  // namespace

std::string autobuildIconKey(const std::vector<uint8_t>& combinedActorInfo) {
    // FNV-1a/64 is sufficient here: this is a stable local content identity,
    // not a security boundary. Include the length so malformed/truncated
    // blobs cannot share the normal 6688-byte namespace accidentally.
    uint64_t hash = 14695981039346656037ULL;
    for (const uint8_t byte : combinedActorInfo) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    hash ^= static_cast<uint64_t>(combinedActorInfo.size());
    hash *= 1099511628211ULL;

    std::ostringstream key;
    key << std::hex << std::setfill('0') << std::setw(16) << hash;
    return key.str();
}

std::string autobuildIconPath(const std::string& blueprintKey) {
    return std::string(kIconDirectory) + "/build_" + blueprintKey + ".jpg";
}

bool saveAutobuildIcon(const std::vector<uint8_t>& combinedActorInfo, const std::vector<uint8_t>& imageBytes) {
    if (combinedActorInfo.empty() || imageBytes.empty()) return false;
    const std::string path = autobuildIconPath(autobuildIconKey(combinedActorInfo));
    if (!totk::ensureParentDirectory(path)) return false;
    return totk::writeFileText(path, std::string(imageBytes.begin(), imageBytes.end()));
}

std::vector<uint8_t> loadAutobuildIcon(const std::vector<uint8_t>& combinedActorInfo) {
    if (combinedActorInfo.empty()) return {};
    const std::string content = totk::loadFileText(autobuildIconPath(autobuildIconKey(combinedActorInfo)));
    return std::vector<uint8_t>(content.begin(), content.end());
}

}  // namespace totk::ui
