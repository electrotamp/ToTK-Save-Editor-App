#include "platform/switch_save_mount.hpp"

#ifndef __SWITCH__

namespace totk {

bool SwitchSaveMount::initialize() { return false; }
void SwitchSaveMount::shutdown() {}
bool SwitchSaveMount::isMounted() { return false; }
std::string SwitchSaveMount::saveRoot() { return {}; }
bool SwitchSaveMount::commit() { return true; }
const std::string& SwitchSaveMount::lastError() {
    static const std::string kError = "Switch save mount is only available on Nintendo Switch.";
    return kError;
}
std::string SwitchSaveMount::mountedTitleIdHex() { return {}; }
void SwitchSaveMount::logIconCacheDiagnostics() {}
bool SwitchSaveMount::invalidateAutobuildIcon(size_t) { return false; }
bool SwitchSaveMount::invalidateAllAutobuildIcons() { return false; }
bool SwitchSaveMount::readAutobuildDraftIcons(const std::vector<size_t>& slotIndexes,
                                              std::vector<std::vector<uint8_t>>& outBytesPerIndex) {
    outBytesPerIndex.assign(slotIndexes.size(), {});
    return false;
}
}  // namespace totk

#else

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <switch.h>
#include <unistd.h>

#include <dirent.h>
#include <sys/stat.h>

#include "util/totk_log.hpp"

namespace totk {

namespace {

constexpr const char* kMountName = "totk";
constexpr const char* kSaveRoot = "totk:/";

std::string gLastError;
bool gMounted = false;
u64 gTitleId = 0;

bool isTotkTitleId(u64 applicationId) {
    return applicationId >= 0x0100F2C0115B6000ULL && applicationId <= 0x0100F2C0115B7FFFULL;
}

bool uidEquals(const AccountUid& a, const AccountUid& b) {
    return a.uid[0] == b.uid[0] && a.uid[1] == b.uid[1];
}

bool saveHasSlotFolders() {
    for (int i = 0; i < 6; ++i) {
        char path[48];
        std::snprintf(path, sizeof(path), "%sslot_0%d/progress.sav", kSaveRoot, i);
        if (access(path, F_OK) == 0) return true;
    }
    return false;
}

Result getPreselectedUser(AccountUid* user) {
    Result rc = accountInitialize(AccountServiceType_System);
    if (R_FAILED(rc)) {
        rc = accountInitialize(AccountServiceType_Application);
        if (R_FAILED(rc)) return rc;
    }

    rc = accountGetPreselectedUser(user);
    accountExit();
    return rc;
}

Result findTotkSaveForUser(const AccountUid& user, u64* outApplicationId) {
    FsSaveDataInfoReader reader{};
    Result rc = fsOpenSaveDataInfoReader(&reader, FsSaveDataSpaceId_User);
    if (R_FAILED(rc)) return rc;

    FsSaveDataInfo info{};
    s64 totalEntries = 0;
    bool found = false;

    while (true) {
        rc = fsSaveDataInfoReaderRead(&reader, &info, 1, &totalEntries);
        if (R_FAILED(rc) || totalEntries == 0) break;
        if (info.save_data_type != FsSaveDataType_Account) continue;
        if (!isTotkTitleId(info.application_id)) continue;
        if (!uidEquals(info.uid, user)) continue;

        *outApplicationId = info.application_id;
        found = true;
        break;
    }

    fsSaveDataInfoReaderClose(&reader);
    return found ? 0 : MAKERESULT(Module_Libnx, LibnxError_NotFound);
}

Result findAnyTotkSave(u64* outApplicationId, AccountUid* outUser) {
    FsSaveDataInfoReader reader{};
    Result rc = fsOpenSaveDataInfoReader(&reader, FsSaveDataSpaceId_User);
    if (R_FAILED(rc)) return rc;

    FsSaveDataInfo info{};
    s64 totalEntries = 0;
    bool found = false;

    while (true) {
        rc = fsSaveDataInfoReaderRead(&reader, &info, 1, &totalEntries);
        if (R_FAILED(rc) || totalEntries == 0) break;
        if (info.save_data_type != FsSaveDataType_Account) continue;
        if (!isTotkTitleId(info.application_id)) continue;

        *outApplicationId = info.application_id;
        *outUser = info.uid;
        found = true;
        break;
    }

    fsSaveDataInfoReaderClose(&reader);
    return found ? 0 : MAKERESULT(Module_Libnx, LibnxError_NotFound);
}

// Recursively logs every entry under `path` (depth-first, bounded to avoid a
// runaway walk on anything unexpected) via TOTK_LOG. Used to check whether
// an icon/thumbnail cache lives directly inside the mounted save data
// archive itself, rather than as a separate FsSaveDataType_Cache archive.
void logDirectoryTree(const std::string& path, int depth) {
    if (depth > 6) {
        TOTK_LOG("icon cache probe:   %s (max depth reached, not descending further)", path.c_str());
        return;
    }

    DIR* dir = opendir(path.c_str());
    if (!dir) return;

    while (dirent* entry = readdir(dir)) {
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) continue;

        const std::string fullPath = path + entry->d_name;
        struct stat st {};
        if (stat(fullPath.c_str(), &st) != 0) {
            TOTK_LOG("icon cache probe:   %s (stat failed)", fullPath.c_str());
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            TOTK_LOG("icon cache probe:   %s/ (dir)", fullPath.c_str());
            logDirectoryTree(fullPath + "/", depth + 1);
        } else {
            TOTK_LOG("icon cache probe:   %s (%lld bytes)", fullPath.c_str(), static_cast<long long>(st.st_size));
        }
    }
    closedir(dir);
}

bool tryMount(u64 applicationId, const AccountUid& user) {
    if (gMounted) fsdevUnmountDevice(kMountName);

    const Result rc = fsdevMountSaveData(kMountName, applicationId, user);
    if (R_FAILED(rc)) {
        gLastError = "fsdevMountSaveData failed: 0x" + std::to_string(static_cast<unsigned>(rc));
        return false;
    }

    if (!saveHasSlotFolders()) {
        fsdevUnmountDevice(kMountName);
        gLastError = "Mounted TotK save data but no slot_00..slot_05 folders were found.";
        return false;
    }

    gMounted = true;
    gTitleId = applicationId;
    gLastError.clear();

    SwitchSaveMount::logIconCacheDiagnostics();

    return true;
}

}  // namespace

bool SwitchSaveMount::initialize() {
    if (gMounted) return true;

    AccountUid user{};
    const bool havePreselectedUser = R_SUCCEEDED(getPreselectedUser(&user));

    u64 applicationId = 0;
    if (havePreselectedUser && R_SUCCEEDED(findTotkSaveForUser(user, &applicationId))) {
        return tryMount(applicationId, user);
    }

    // No preselected user (e.g. launched directly from the Homebrew Menu without
    // going through the console's profile-selection flow first) — fall back to
    // scanning save data directly, which discovers the owning user itself and
    // doesn't depend on account services at all.
    AccountUid discoveredUser{};
    if (R_SUCCEEDED(findAnyTotkSave(&applicationId, &discoveredUser))) {
        return tryMount(applicationId, discoveredUser);
    }

    if (havePreselectedUser) {
        static const u64 kKnownTitleIds[] = {
            0x0100F2C0115B6000ULL,  // US
            0x0100F2C0115B6800ULL,  // EU
            0x0100F2C0115B7000ULL,  // JP
            0x0100F2C0115B7800ULL,  // CN/HK/TW/KR
        };

        for (u64 titleId : kKnownTitleIds) {
            if (tryMount(titleId, user)) return true;
        }
    }

    gLastError = havePreselectedUser
        ? "No Tears of the Kingdom save data found for the active user."
        : "No Tears of the Kingdom save data found on this console.";
    return false;
}

void SwitchSaveMount::shutdown() {
    if (gMounted) {
        fsdevUnmountDevice(kMountName);
        gMounted = false;
        gTitleId = 0;
    }
}

bool SwitchSaveMount::isMounted() { return gMounted; }

std::string SwitchSaveMount::saveRoot() { return gMounted ? kSaveRoot : std::string{}; }

bool SwitchSaveMount::commit() {
    if (!gMounted) return false;
    const Result rc = fsdevCommitDevice(kMountName);
    if (R_FAILED(rc)) {
        gLastError = "Failed to commit save changes: 0x" + std::to_string(static_cast<unsigned>(rc));
        return false;
    }
    return true;
}

const std::string& SwitchSaveMount::lastError() { return gLastError; }

// FsSaveDataInfoReader-based enumeration (tried first, 2026-08-17) came back
// with zero FsSaveDataType_Cache entries for this title even scanning every
// FsSaveDataSpaceId — turned out to be the wrong approach entirely. JKSV
// found and exported a real one (icon/Draft_NNN.png, matching the
// AutoBuilder.Draft.Content.* hash family, plus Weapon_NNN.png/
// Shield_NNN.png — confirmed 2026-08-18). fsdevMountCacheStorage() is the
// dedicated, correct API (mirrors fsdevMountSaveData(), just for Cache type)
// and needs no enumeration at all — just the application_id, already known,
// plus a save_data_index. Index 0 is what essentially every game with a
// single cache storage uses; a handful of fallback indices are tried in
// case TotK is unusual.
constexpr const char* kCacheMountName = "totkcache";
constexpr const char* kCacheRoot = "totkcache:/";

// Tries mounting the title's Cache-type save archive (indices 0-3) and, if
// successful, calls `fn` with it mounted at kCacheRoot before unmounting.
// Returns false (fn not called) if no Cache archive could be mounted at all.
template <typename Fn>
bool withCacheStorageMounted(u64 titleId, Fn&& fn) {
    for (u16 index = 0; index < 4; ++index) {
        const Result rc = fsdevMountCacheStorage(kCacheMountName, titleId, index);
        if (R_SUCCEEDED(rc)) {
            fn();
            fsdevUnmountDevice(kCacheMountName);
            return true;
        }
    }
    return false;
}

void SwitchSaveMount::logIconCacheDiagnostics() {
    if (!gMounted) return;

    // Part 1: the mounted Account save archive itself (totk:/) — list
    // everything, recursively, in case an icon/preview cache lives directly
    // inside the save data this app already has (it doesn't, confirmed
    // 2026-08-17 — only slot_XX/, album/, picturebook/, option.sav,
    // storage/CacheStorageKey.dat — but kept here as a standing check).
    TOTK_LOG("icon cache probe: full %s tree —", kSaveRoot);
    logDirectoryTree(kSaveRoot, 0);

    // Part 2: the title's actual Cache-type save archive.
    const bool mounted = withCacheStorageMounted(gTitleId, [] {
        TOTK_LOG("icon cache probe: full %s tree (FsSaveDataType_Cache) —", kCacheRoot);
        logDirectoryTree(kCacheRoot, 0);
    });
    if (!mounted) {
        TOTK_LOG("icon cache probe: fsdevMountCacheStorage failed for indices 0-3 — no accessible Cache "
                  "archive for this title.");
    }
}

bool SwitchSaveMount::invalidateAutobuildIcon(size_t draftPosition) {
    if (!gMounted) return false;

    char path[64];
    std::snprintf(path, sizeof(path), "%sicon/Draft_%03zu.png", kCacheRoot, draftPosition);

    bool removed = false;
    const bool mounted = withCacheStorageMounted(gTitleId, [&] {
        if (access(path, F_OK) != 0) {
            TOTK_LOG("icon cache: %s not present, nothing to invalidate", path);
            return;
        }
        if (std::remove(path) == 0) {
            TOTK_LOG("icon cache: removed stale %s", path);
            removed = true;
            const Result commitRc = fsdevCommitDevice(kCacheMountName);
            if (R_FAILED(commitRc)) {
                TOTK_LOG("icon cache: CacheStorage commit failed: 0x%x", commitRc);
            }
        } else {
            TOTK_LOG("icon cache: failed to remove %s", path);
        }
    });
    if (!mounted) {
        TOTK_LOG("icon cache: could not mount CacheStorage to invalidate draft %zu", draftPosition);
    }
    return removed;
}

bool SwitchSaveMount::invalidateAllAutobuildIcons() {
    if (!gMounted) return false;

    int removedCount = 0;
    const bool mounted = withCacheStorageMounted(gTitleId, [&] {
        const std::string iconDir = std::string(kCacheRoot) + "icon/";
        DIR* dir = opendir(iconDir.c_str());
        if (!dir) {
            TOTK_LOG("icon cache: %s not present, nothing to clear", iconDir.c_str());
            return;
        }

        while (dirent* entry = readdir(dir)) {
            if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) continue;
            const std::string fullPath = iconDir + entry->d_name;
            struct stat st {};
            if (stat(fullPath.c_str(), &st) != 0 || S_ISDIR(st.st_mode)) continue;  // icon/ is flat; skip any subdir

            if (std::remove(fullPath.c_str()) == 0) {
                ++removedCount;
            } else {
                TOTK_LOG("icon cache: failed to remove %s", fullPath.c_str());
            }
        }
        closedir(dir);

        TOTK_LOG("icon cache: cleared %d cached icon(s) from %s — every equipment/Autobuild thumbnail will "
                  "re-render next time it's viewed in-game.",
                  removedCount, iconDir.c_str());

        if (removedCount > 0) {
            const Result commitRc = fsdevCommitDevice(kCacheMountName);
            if (R_FAILED(commitRc)) {
                TOTK_LOG("icon cache: CacheStorage commit failed: 0x%x", commitRc);
            }
        }
    });
    if (!mounted) {
        TOTK_LOG("icon cache: could not mount CacheStorage to clear icons");
        return false;
    }
    return removedCount > 0;
}

bool SwitchSaveMount::readAutobuildDraftIcons(const std::vector<size_t>& slotIndexes,
                                              std::vector<std::vector<uint8_t>>& outBytesPerIndex) {
    outBytesPerIndex.assign(slotIndexes.size(), {});
    if (!gMounted) return false;

    const bool mounted = withCacheStorageMounted(gTitleId, [&] {
        size_t newestDraftId = 0;
        bool foundAnyDraft = false;
        if (DIR* dir = opendir((std::string(kCacheRoot) + "icon").c_str())) {
            while (dirent* entry = readdir(dir)) {
                size_t id = 0;
                char trailing = '\0';
                if (std::sscanf(entry->d_name, "Draft_%zu.png%c", &id, &trailing) == 1) {
                    newestDraftId = foundAnyDraft ? std::max(newestDraftId, id) : id;
                    foundAnyDraft = true;
                }
            }
            closedir(dir);
        }
        if (!foundAnyDraft) {
            TOTK_LOG("icon cache: no Draft_NNN.png files found; no Autobuild thumbnails can be resolved");
            return;
        }

        // Invalid saves produced by older editor experiments can contain a
        // duplicate Index. There is no principled way to decide which of two
        // different blueprints owns the one resolved cache image, so leave
        // every duplicate blank (a content-keyed SD preview may still load).
        std::unordered_map<size_t, size_t> indexCounts;
        for (const size_t index : slotIndexes) ++indexCounts[index];

        TOTK_LOG("icon cache: resolver newest=%zu current_window=%zu..%zu requests=%zu", newestDraftId,
                 newestDraftId >= 29 ? newestDraftId - 29 : 0, newestDraftId, slotIndexes.size());
        for (size_t i = 0; i < slotIndexes.size(); ++i) {
            const size_t slotIndex = slotIndexes[i];
            if (slotIndex >= 30 || slotIndex > newestDraftId) {
                TOTK_LOG("icon cache: invalid Index=%zu (newest=%zu), skipping", slotIndex, newestDraftId);
                continue;
            }
            if (indexCounts[slotIndex] > 1) {
                TOTK_LOG("icon cache: duplicate Index=%zu appears %zu times, refusing ambiguous cache fallback",
                         slotIndex, indexCounts[slotIndex]);
                continue;
            }

            const size_t draftId = newestDraftId - slotIndex;
            char path[64];
            std::snprintf(path, sizeof(path), "%sicon/Draft_%03zu.png", kCacheRoot, draftId);

            FILE* file = std::fopen(path, "rb");
            if (!file) {
                TOTK_LOG("icon cache: Index=%zu -> draftId=%zu not found", slotIndex, draftId);
                continue;
            }
            std::fseek(file, 0, SEEK_END);
            const long size = std::ftell(file);
            // Real in-game renders seen in the wild run 7KB-90KB. A handful of
            // stray 303-byte files were found in this switch's cache under
            // filenames that coincidentally match array positions 0-29 -
            // almost certainly leftover debris from the now-removed direct-
            // write experiment (writeAutobuildDraftIcon), not real thumbnails.
            // Treat anything implausibly small as corrupt/placeholder rather
            // than displaying it as if it were a real cached render.
            constexpr long kMinPlausibleIconBytes = 2048;
            if (size < kMinPlausibleIconBytes) {
                TOTK_LOG("icon cache: skipping implausibly small %s (%ld bytes) - treating as corrupt/placeholder",
                         path, size);
                std::fclose(file);
                continue;
            }
            std::fseek(file, 0, SEEK_SET);

            std::vector<uint8_t> bytes(static_cast<size_t>(size));
            const size_t readCount = std::fread(bytes.data(), 1, bytes.size(), file);
            std::fclose(file);
            if (readCount == bytes.size()) {
                TOTK_LOG("icon cache: Index=%zu -> draftId=%zu found (%ld bytes)", slotIndex, draftId, size);
                outBytesPerIndex[i] = std::move(bytes);
            }
        }
    });
    if (!mounted) {
        TOTK_LOG("icon cache: could not mount CacheStorage to read %zu draft icon(s)", slotIndexes.size());
    }
    return mounted;
}

std::string SwitchSaveMount::mountedTitleIdHex() {
    if (!gMounted) return {};
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "0x%016lX", static_cast<unsigned long>(gTitleId));
    return buffer;
}

}  // namespace totk

#endif
