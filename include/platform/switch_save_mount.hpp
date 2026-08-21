#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace totk {

// Mounts the active user's Zelda: TOTK save data on Switch (totk:/).
class SwitchSaveMount {
public:
    // Mount TotK save data for the active user. Safe to call multiple times.
    static bool initialize();
    static void shutdown();

    static bool isMounted();
    static std::string saveRoot();
    static bool commit();

    static const std::string& lastError();
    static std::string mountedTitleIdHex();

    // Logs (via TOTK_LOG) the mounted TotK title's FsSaveDataType_Cache
    // archive contents — confirmed present on real hardware 2026-08-18
    // (found via JKSV after this app's own FsSaveDataInfoReader-based scan
    // wrongly came back empty; see logDirectoryTree's caller for why).
    // Read-only. Safe no-op if nothing is mounted yet.
    static void logIconCacheDiagnostics();

    // Removes icon/Draft_<draftPosition, zero-padded to 3 digits>.png from
    // the title's Cache-type save archive, if present, and commits.
    // draftPosition is the AutoBuilder draft array position just written
    // (SaveEditor::importAutobuilderFavorite/importAutobuilderHistory's
    // outDraftPosition) — the
    // game keys its per-draft cached History/Favorites thumbnail render by
    // that same position, and confirmed on real hardware 2026-08-18 that a
    // stale/blank cached render there does not get regenerated on its own.
    // Returns true only if a stale file was actually found and removed;
    // false (harmlessly) if there was nothing to remove or the archive
    // couldn't be reached. Never fails/touches anything if the position had
    // no cached icon yet — this can't make a working icon worse, only clear
    // a broken one so the game re-renders it.
    static bool invalidateAutobuildIcon(size_t draftPosition);

    // Broader hammer: removes every file directly under icon/ in the
    // Cache-type archive (Draft_*/Weapon_*/Shield_* — every cached
    // thumbnail this title has ever rendered, not just Autobuild's),
    // then commits. Confirmed 2026-08-18 that a targeted single-file
    // delete isn't sufficient by itself for a draft position with no
    // prior cached icon at all (nothing to delete, still rendered blank)
    // — there may be a separate storage/CacheStorageInfo.dat tracking
    // which positions have been rendered, independent of whether the PNG
    // itself exists, that a single-file delete can't reset. Safe: every
    // entry here is a regenerable render the game recreates next time
    // that item/build is viewed, and nothing else reads these files.
    static bool invalidateAllAutobuildIcons();

    // Resolves and reads the cached thumbnail for each requested
    // AutoBuilder.Draft.Content.Index value. Draft_NNN filenames use an
    // absolute lifetime creation counter while Index is the current
    // 30-entry ring age (0=newest, 29=oldest). Controlled before/after
    // captures on 2026-08-19 proved the association:
    //
    //   NNN = highest Draft filename currently present - Index
    //
    // A normal new build increments the highest filename, advances every
    // Index modulo 30, and replaces Index 29 as the new Index 0. This method
    // enumerates the cache once, applies that mapping, and ignores stale
    // Draft files outside the current 30-ID window.
    //
    // outBytesPerPosition matches the input keys in size and order; a key
    // with no cached render (never viewed in-game, or implausibly small —
    // see the 2048-byte corrupt/placeholder guard in the .cpp) is left as an
    // empty vector rather than failing the whole read. Returns false only if
    // the Cache archive itself couldn't be mounted at all — check individual
    // entries for per-icon misses.
    static bool readAutobuildDraftIcons(const std::vector<size_t>& slotIndexes,
                                        std::vector<std::vector<uint8_t>>& outBytesPerIndex);
};

}  // namespace totk
