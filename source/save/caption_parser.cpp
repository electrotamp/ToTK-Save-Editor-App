#include "save/caption_parser.hpp"

#include "save/location_names.hpp"
#include "save/marc_file.hpp"
#include "util/totk_log.hpp"

#include <cctype>
#include <cstdio>

namespace totk {

namespace {

constexpr uint32_t kHashYear = 0x9811A3F7u;
constexpr uint32_t kHashMinute = 0x27853BF7u;
constexpr uint32_t kHashHour = 0x23F3D75Eu;
constexpr uint32_t kHashMonth = 0xDFD840D3u;
constexpr uint32_t kHashDay = 0xBD46F485u;
constexpr uint32_t kHashAutosave = 0x25F03CAAu;
constexpr uint32_t kHashJpeg = 0x63696a32u;
// Points to a fixed 64-byte block: locationId, then mapLayer immediately
// after locationId's null terminator (both plain null-terminated ASCII, not
// length-prefixed) — verified 2026-08-11 against a real caption.sav.
constexpr uint32_t kHashLocation = 0xF74E0E8Eu;

bool readBoolByHash(const MarcFile& file, uint32_t targetHash, bool& out) {
    for (size_t offset = 0x28; offset < 0x1C0; offset += 8) {
        if (file.readU32(offset) != targetHash) continue;
        out = file.readU8(offset + 4) == 1;
        return true;
    }
    return false;
}

bool readU32ByHash(const MarcFile& file, uint32_t targetHash, uint32_t& out) {
    for (size_t offset = 0x28; offset < 0x1C0; offset += 8) {
        if (file.readU32(offset) != targetHash) continue;
        out = file.readU32(offset + 4);
        return true;
    }
    return false;
}

}  // namespace

bool CaptionParser::loadFromFile(const std::string& path, CaptionMetadata& out, bool includeThumbnail) {
    out = CaptionMetadata{};
    MarcFile file;
    if (!file.loadFromFile(path)) return false;

    uint32_t year = 0;
    uint32_t month = 0;
    uint32_t day = 0;
    uint32_t hour = 0;
    uint32_t minute = 0;
    if (!readU32ByHash(file, kHashYear, year) || !readU32ByHash(file, kHashMonth, month) ||
        !readU32ByHash(file, kHashDay, day) || !readU32ByHash(file, kHashHour, hour) ||
        !readU32ByHash(file, kHashMinute, minute)) {
        return false;
    }

    bool autosave = false;
    readBoolByHash(file, kHashAutosave, autosave);

    out.valid = true;
    out.year = static_cast<int>(year);
    out.month = static_cast<int>(month);
    out.day = static_cast<int>(day);
    out.hour = static_cast<int>(hour);
    out.minute = static_cast<int>(minute);
    out.autosave = autosave;
    out.sortKey = static_cast<int64_t>(year) * 100000000LL + static_cast<int64_t>(month) * 1000000LL +
                  static_cast<int64_t>(day) * 10000LL + static_cast<int64_t>(hour) * 100LL +
                  static_cast<int64_t>(minute);

    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d", out.year, out.month, out.day, out.hour,
                  out.minute);
    out.formattedDate = buffer;

    uint32_t locationOffset = 0;
    if (readU32ByHash(file, kHashLocation, locationOffset) && locationOffset < file.size()) {
        constexpr size_t kLocationBlockBytes = 64;
        out.locationId = file.readString(locationOffset, kLocationBlockBytes);
        const size_t layerOffset = static_cast<size_t>(locationOffset) + out.locationId.size() + 1;
        if (layerOffset < file.size()) {
            out.mapLayer = file.readString(layerOffset, kLocationBlockBytes - (out.locationId.size() + 1));
        }
    }

    if (includeThumbnail) {
        uint32_t jpegOffset = 0;
        if (readU32ByHash(file, kHashJpeg, jpegOffset) && jpegOffset + 4 < file.size()) {
            const uint32_t jpegSize = file.readU32(jpegOffset);
            constexpr uint32_t kMaxJpegBytes = 512u * 1024u;
            const size_t start = static_cast<size_t>(jpegOffset) + 4;
            if (jpegSize > 0 && jpegSize <= kMaxJpegBytes && start < file.size()) {
                const size_t end = start + static_cast<size_t>(jpegSize);
                if (end <= file.size() && end > start) {
                    out.thumbnailJpeg = file.readBytes(start, jpegSize);
                    if (out.thumbnailJpeg.size() < 3 || out.thumbnailJpeg[0] != 0xFF ||
                        out.thumbnailJpeg[1] != 0xD8 || out.thumbnailJpeg[2] != 0xFF) {
                        out.thumbnailJpeg.clear();
                    }
                }
            }
        }
    }

    return true;
}

std::string CaptionParser::displayLocationName(const CaptionMetadata& meta) {
    if (meta.locationId.empty()) return {};

    // Prefer the real in-game name from the extracted message-archive table
    // (see location_names.hpp) over the heuristic prettifier below, which is
    // only a rough approximation of the actual localized text.
    const auto& table = LocationNames::instance();
    if (meta.mapLayer == "Sky" || meta.mapLayer == "Underground") {
        const std::string layered = table.lookup(meta.locationId + "_" + meta.mapLayer);
        if (!layered.empty()) return layered;
    }
    const std::string exact = table.lookup(meta.locationId);
    if (!exact.empty()) return exact;

    std::string s = meta.locationId;

    // Strip a recognized category prefix, if present.
    static const char* kPrefixes[] = {"Cave_", "Dungeon_", "Field_", "Castle_", "Town_", "Room_"};
    for (const char* prefix : kPrefixes) {
        const size_t len = std::char_traits<char>::length(prefix);
        if (s.size() > len && s.compare(0, len, prefix) == 0) {
            s = s.substr(len);
            break;
        }
    }

    // Drop a trailing "_<digits>" instance-number suffix (e.g. the "_0018"
    // distinguishing this specific cave from every other "CentralHyrule"
    // cave) — not meaningful to a player.
    const size_t lastUnderscore = s.find_last_of('_');
    if (lastUnderscore != std::string::npos && lastUnderscore + 1 < s.size()) {
        bool allDigits = true;
        for (size_t i = lastUnderscore + 1; i < s.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
                allDigits = false;
                break;
            }
        }
        if (allDigits) s = s.substr(0, lastUnderscore);
    }

    // Split CamelCase / camelCase into separate words ("CentralHyrule" ->
    // "Central Hyrule"), and turn remaining underscores into spaces.
    std::string pretty;
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '_') {
            if (!pretty.empty() && pretty.back() != ' ') pretty += ' ';
            continue;
        }
        if (i > 0 && std::isupper(static_cast<unsigned char>(c)) && std::islower(static_cast<unsigned char>(s[i - 1]))) {
            pretty += ' ';
        }
        pretty += c;
    }
    if (pretty.empty()) pretty = meta.locationId;

    // Map layer qualifier — only worth showing for the non-default layers;
    // "Ground" (ordinary surface) doesn't need calling out.
    if (meta.mapLayer == "Underground") {
        pretty += " (Depths)";
    } else if (meta.mapLayer == "Sky") {
        pretty += " (Sky)";
    }

    return pretty;
}

void CaptionParser::debugDumpHashTable(const std::string& path) {
    MarcFile file;
    if (!file.loadFromFile(path)) {
        TOTK_LOG("caption dump: failed to open %s", path.c_str());
        return;
    }
    TOTK_LOG("caption dump: === %s (size=%zu) ===", path.c_str(), file.size());

    for (size_t offset = 0x28; offset < 0x1C0; offset += 8) {
        const uint32_t hash = file.readU32(offset);
        const uint32_t value = file.readU32(offset + 4);
        if (hash == 0 && value == 0) continue;  // empty/unused slot

        const char* known = "?";
        if (hash == kHashYear) known = "Year";
        else if (hash == kHashMonth) known = "Month";
        else if (hash == kHashDay) known = "Day";
        else if (hash == kHashHour) known = "Hour";
        else if (hash == kHashMinute) known = "Minute";
        else if (hash == kHashAutosave) known = "Autosave";
        else if (hash == kHashJpeg) known = "JpegOffset";

        TOTK_LOG("caption dump: off=0x%03zx hash=0x%08X value=%u (0x%08X) known=%s", offset, hash, value, value,
                  known);

        // Unknown slot: try treating value as an offset to a length-prefixed
        // blob — the same layout the JPEG field uses (hash -> offset, then a
        // u32 length followed by that many bytes at that offset) — and peek
        // for short, printable text. A location name would very likely be
        // stored this way if it's stored at all, since embedded strings in
        // this save format aren't fixed-width.
        if (known[0] == '?' && static_cast<size_t>(value) + 4 < file.size()) {
            const uint32_t len = file.readU32(value);
            if (len > 0 && len <= 64) {
                const size_t start = static_cast<size_t>(value) + 4;
                if (start + len <= file.size()) {
                    const auto bytes = file.readBytes(start, len);
                    bool printable = true;
                    for (uint8_t b : bytes) {
                        if (b != 0 && (b < 0x20 || b > 0x7E)) {
                            printable = false;
                            break;
                        }
                    }
                    if (printable) {
                        const std::string text(bytes.begin(), bytes.end());
                        TOTK_LOG("caption dump:   -> candidate text @offset=%u len=%u: \"%s\"", value, len,
                                  text.c_str());
                    }
                }
            }
        }
    }

    TOTK_LOG("caption dump: === end %s ===", path.c_str());
}

}  // namespace totk
