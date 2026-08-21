#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace totk {

struct CaptionMetadata {
    bool valid = false;
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    bool autosave = false;
    int64_t sortKey = 0;
    std::string formattedDate;
    std::vector<uint8_t> thumbnailJpeg;

    // Raw internal location id (e.g. "Cave_CentralHyrule_0018") and map layer
    // ("Ground"/"Sky"/"Underground") the save was taken on. Confirmed present
    // by hex-dumping a real caption.sav on 2026-08-11 — hash 0xF74E0E8E points
    // to a fixed 64-byte block holding these as two back-to-back
    // null-terminated ASCII strings (id first, then layer immediately after
    // id's null terminator); not length-prefixed like the embedded JPEG is,
    // which is why the original hash-table scan missed it. Empty if this
    // save's caption predates the field or the layout ever changes.
    std::string locationId;
    std::string mapLayer;
};

class CaptionParser {
public:
    static bool loadFromFile(const std::string& path, CaptionMetadata& out, bool includeThumbnail = true);

    // Turns locationId/mapLayer into a display string, e.g.
    // "Cave_CentralHyrule_0018" + "Underground" -> "Central Hyrule (Depths)".
    // Best-effort prettification (strip a known category prefix, drop a
    // trailing _<digits> instance suffix, split CamelCase) since there's no
    // id -> proper-name lookup table for the ~hundreds of locations in the
    // game — this is not the same as the actual in-game name in every case,
    // just a readable rendering of the internal id. Returns empty if
    // locationId is empty.
    static std::string displayLocationName(const CaptionMetadata& meta);

    // Debug-only: logs every raw hash/value slot actually present in the
    // caption's hash table via TOTK_LOG (visible over nxlink), not just the
    // 7 fields loadFromFile knows about — plus a best-effort decode attempt
    // for any unknown slot whose value looks like a valid offset into a
    // length-prefixed text blob (the same layout the embedded JPEG uses).
    // For investigating whether an undocumented field (e.g. a location name)
    // exists that neither this parser nor the community reference editor
    // currently reads.
    static void debugDumpHashTable(const std::string& path);
};

}  // namespace totk
