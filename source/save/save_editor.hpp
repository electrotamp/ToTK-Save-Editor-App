#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "save/autobuilder_cai.hpp"
#include "save/item_types.hpp"
#include "save/marc_file.hpp"
#include "save/variable_store.hpp"

namespace totk {

// One entry per real in-game Favorites button (0-7 — the game only ever
// shows 8, confirmed by the user against real hardware 2026-08-19).
// `favoriteIndex` is this app's own display ordinal, NOT written to the save.
// Favorites are presented in AutoBuilder.Draft.Content.Index order (0 is
// newest). Controlled save/cache pairs confirmed Index is the relative age
// inside TotK's 30-entry history ring, not the cache filename itself.
// `draftPosition` is the index into autobuilderSlots()/the raw
// Index/CombinedActorInfo/CameraPos/CameraAt arrays currently holding this
// favorite, valid only when `occupied` is true.
struct AutobuilderFavoriteSlot {
    int favoriteIndex = -1;
    bool occupied = false;
    size_t draftPosition = 0;
};

struct GameVersion {
    std::string label;
    size_t fileSize;
    uint32_t header;
    uint32_t metaDataStart;
};

struct SaveSlotInfo {
    int slotIndex = -1;
    std::string captionPath;
    std::string progressPath;
    std::string dateLabel;
    std::string formattedDate;
    int64_t sortKey = 0;
    bool autosave = false;
    bool hasCaption = false;
    std::vector<uint8_t> thumbnailJpeg;
    std::string locationName;  // best-effort display text; empty if unavailable
};

struct PlayerStats {
    std::string playtime;
    uint32_t rupees = 0;
    uint32_t maxHearts = 0;
    uint32_t maxStamina = 0;
    float maxBattery = 0;
    uint32_t ponyPoints = 0;
    uint32_t pouchWeapons = 0;
    uint32_t pouchBows = 0;
    uint32_t pouchShields = 0;
    Vec3 savePos{};
    uint32_t parasailPattern = 0;
    bool ultraHand = false;
    bool fusion = false;
    bool ascend = false;
    bool recall = false;
    bool autobuilder = false;
    bool amiibo = false;
    int mapPinCount = 0;
};

class SaveEditor {
public:
    SaveEditor();

    bool loadProgress(const std::string& path);
    bool saveProgress(const std::string& path);
    bool isLoaded() const { return loaded_; }

    const PlayerStats& stats() const { return stats_; }
    PlayerStats& stats() { return stats_; }

    std::string gameVersion() const { return gameVersion_; }
    bool isGameMod() const { return gameMod_; }

    std::map<std::string, std::vector<EquipmentItem>>& equipment() { return equipment_; }
    std::map<std::string, std::vector<StackItem>>& stackItems() { return stackItems_; }
    std::vector<ArmorItem>& armors() { return armors_; }
    std::vector<HorseItem>& horses() { return horses_; }
    std::vector<AutoBuilderSlot>& autobuilderSlots() { return autobuilderSlots_; }

    void reloadFromFile();
    void applyStatsToFile();
    void saveAllPouches();
    void saveAutobuilder();

    // The real in-game Favorites cap — the game only ever shows 8 Favorites
    // buttons (confirmed by the user against real hardware 2026-08-19).
    // Structurally enforced by autobuilderFavoriteSlots() only ever
    // returning this many entries — there is no separate counter to keep in
    // sync.
    static constexpr int kMaxAutobuildFavorites = 8;

    // The up-to-8 real in-game Favorites buttons, each reporting whether a
    // draft position currently holds it, ordered by Index (newest first),
    // matching the game's logical Autobuild ordering.
    // UI slot pickers should present these, not raw autobuilderSlots()
    // positions, and can never present more than kMaxAutobuildFavorites
    // choices — that's what makes the 8-favorite cap structural rather than
    // a separate check that could drift out of sync.
    std::vector<AutobuilderFavoriteSlot> autobuilderFavoriteSlots() const;

    // Inserts `blueprint` as TotK inserts a normal newly-created history
    // entry: overwrite the oldest unfavorited record, advance every younger
    // Index by one, and make the inserted record Index 0. Older favorites
    // remain protected. If the
    // selected favorite is occupied its old record is unfavorited first;
    // the new record becomes the selected replacement favorite. This keeps
    // the Index permutation and cache-age association intact.
    bool importAutobuilderFavorite(int favoriteIndex, const AutobuilderCai& blueprint, std::string& error,
                                    size_t* outDraftPosition = nullptr);

    // Inserts `blueprint` as the newest unfavorited History entry using the
    // same proven ring transition. `historyDraftPosition` is retained for
    // API compatibility and diagnostics; callers should pass -1 because a
    // natural insertion always evicts Index 29 rather than an arbitrary
    // physical array position.
    bool importAutobuilderHistory(long historyDraftPosition, const AutobuilderCai& blueprint, std::string& error,
                                   size_t* outDraftPosition = nullptr);

    bool restoreAllWeaponDecay();
    bool restoreAllDurability(const std::string& category);
    bool unlockAllCompletion(const std::string& category);
    bool pinMissingLocations(const std::string& category, int limit);
    bool removeAllMapPins();
    // Count of CompletismData::hashesFor(category) whose flag reads nonzero
    // in this save. Indexes the whole hash table in one pass on first call
    // (see VariableStore::indexAllHashes) rather than one lookup at a time —
    // matters here since categories like koroks_hidden run into the
    // hundreds of hashes.
    int countCompletedHashes(const std::string& category) const;

    bool addEquipmentItem(const std::string& category, const std::string& id);
    bool addStackItem(const std::string& category, const std::string& id);
    bool addArrows();
    bool hasArrows() const;
    bool addArmorItem(const std::string& id);
    bool addHorseItem(const std::string& id);
    bool removeEquipmentItem(const std::string& category, size_t index);
    bool removeStackItem(const std::string& category, size_t index);
    bool removeArmorItem(size_t index);
    bool removeHorseItem(size_t index);
    size_t equipmentCapacity(const std::string& category) const;
    size_t stackSlotCapacity(const std::string& category) const;
    size_t armorCapacity() const;
    size_t horseCapacity() const;

    VariableStore& vars() { return *vars_; }
    MarcFile& file() { return file_; }

    static std::vector<SaveSlotInfo> scanSaveDirectory(const std::string& basePath);
    static std::string defaultSaveRoot();

private:
    bool insertAutobuilderNewest(const AutobuilderCai& blueprint, bool favorite, std::string& error,
                                 size_t* outDraftPosition);
    struct HashEntry {
        uint32_t hash;
        std::string name;
        bool isPointer;
        // When true, resolveOffsets() tolerates this hash being absent from
        // the save's own hash table instead of failing the whole load.
        // Used for fields sourced from community hash lists that haven't
        // been empirically confirmed present across every save/game
        // version yet — see AutoBuilder.Draft.Content.IsFavorite.
        bool optional = false;
    };

    bool hasHash(const std::string& key) const { return offsets_.count(key) > 0; }

    MarcFile file_;
    std::unique_ptr<VariableStore> vars_;
    bool loaded_ = false;
    bool gameMod_ = false;
    std::string gameVersion_;
    std::string loadedPath_;
    PlayerStats stats_;
    size_t guidsArrayOffset_ = 0;
    std::vector<uint64_t> guids_;
    std::unordered_map<std::string, size_t> offsets_;

    std::map<std::string, std::vector<EquipmentItem>> equipment_;
    std::map<std::string, std::vector<StackItem>> stackItems_;
    std::vector<ArmorItem> armors_;
    std::vector<HorseItem> horses_;
    std::vector<AutoBuilderSlot> autobuilderSlots_;

    static const std::vector<GameVersion> kGameVersions;
    static const std::vector<HashEntry> kCoreHashes;

    bool validateAndIndex();
    bool resolveOffsets();
    void loadGuids();
    void loadStats();
    void loadPouches();
    void loadAutobuilder();
    void saveHorses();
    size_t offsetFor(const std::string& key) const;
    size_t pointerFor(const std::string& key) const;
};

}  // namespace totk
