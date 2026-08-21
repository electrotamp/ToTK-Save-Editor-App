#include "save/horse_defaults.hpp"

#include "save/game_limits.hpp"
#include "save/murmur_hash.hpp"
#include "save/variable_store.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace totk {
namespace HorseDefaults {

namespace {

constexpr uint64_t kMinGeneratedUid = 0x10000;

uint32_t noneEnumHash() {
    return murmurHash3("None");
}

bool isNoneEnum(uint32_t value) {
    return value == 0 || value == noneEnumHash();
}

bool isNormalTamableBreed(const std::string& id) {
    if (id == "GameRomHorse") return true;
    if (id.rfind("GameRomHorse", 0) != 0 || id.size() <= 12) return false;
    const std::string suffix = id.substr(12);
    return !suffix.empty() && std::all_of(suffix.begin(), suffix.end(), ::isdigit);
}

void clearPortraitColors(HorseItem& item) {
    item.iconPrimaryColorRed = 0;
    item.iconPrimaryColorGreen = 0;
    item.iconPrimaryColorBlue = 0;
    item.iconSecondaryColorRed = 0;
    item.iconSecondaryColorGreen = 0;
    item.iconSecondaryColorBlue = 0;
    item.iconNoseColorRed = 0;
    item.iconNoseColorGreen = 0;
    item.iconNoseColorBlue = 0;
    item.iconHairPrimaryColorRed = 0;
    item.iconHairPrimaryColorGreen = 0;
    item.iconHairPrimaryColorBlue = 0;
    item.iconHairSecondaryColorRed = 0;
    item.iconHairSecondaryColorGreen = 0;
    item.iconHairSecondaryColorBlue = 0;
}

void applyCleanCreateDefaults(HorseItem& item) {
    item.bond = 0.0f;
    item.bondChecked = false;
    item.strength = 100;
    item.speed = 1;
    item.stamina = 2;
    item.pull = 1;
    item.footType = 0;
    item.colorType = 1;
    item.roomId = 0;
    item.amiiboUid = 0;

    item.iconPattern = 0;
    item.iconEyeColor = 0;
    clearPortraitColors(item);

    item.mane = 0;
    item.saddle = 0;
    item.reins = 0;
}

void ensureSpawnMinimums(HorseItem& item) {
    if (item.strength < 100) item.strength = 100;
    if (item.speed < 1) item.speed = 1;
    if (item.stamina < 2) item.stamina = 2;
    if (item.pull < 1) item.pull = 1;
}

void applySpottedPortraitColors(HorseItem& item) {
    item.iconPattern = murmurHash3("05");
    item.iconEyeColor = murmurHash3("Black");
    item.iconPrimaryColorRed = 2;
    item.iconPrimaryColorGreen = 3;
    item.iconPrimaryColorBlue = 2;
    item.iconSecondaryColorRed = 51;
    item.iconSecondaryColorGreen = 41;
    item.iconSecondaryColorBlue = 29;
    item.iconNoseColorRed = 42;
    item.iconNoseColorGreen = 32;
    item.iconNoseColorBlue = 23;
    item.iconHairPrimaryColorRed = 255;
    item.iconHairPrimaryColorGreen = 255;
    item.iconHairPrimaryColorBlue = 255;
    item.iconHairSecondaryColorRed = 3;
    item.iconHairSecondaryColorGreen = 3;
    item.iconHairSecondaryColorBlue = 3;
}

void applyEponaPortraitColors(HorseItem& item) {
    item.iconPattern = murmurHash3("01");
    item.iconEyeColor = murmurHash3("Black");
    item.iconPrimaryColorRed = 14;
    item.iconPrimaryColorGreen = 5;
    item.iconPrimaryColorBlue = 3;
    item.iconSecondaryColorRed = 168;
    item.iconSecondaryColorGreen = 149;
    item.iconSecondaryColorBlue = 104;
    item.iconNoseColorRed = 5;
    item.iconNoseColorGreen = 4;
    item.iconNoseColorBlue = 3;
    item.iconHairPrimaryColorRed = 255;
    item.iconHairPrimaryColorGreen = 255;
    item.iconHairPrimaryColorBlue = 255;
    item.iconHairSecondaryColorRed = 197;
    item.iconHairSecondaryColorGreen = 179;
    item.iconHairSecondaryColorBlue = 136;
}

void applyStableTackDefaults(HorseItem& item) {
    if (isNoneEnum(item.saddle)) {
        item.saddle = murmurHash3("GameRomHorseSaddle_00");
    }
    if (isNoneEnum(item.reins)) {
        item.reins = murmurHash3("GameRomHorseReins_00");
    }
    if (isNoneEnum(item.mane) && item.horseType == 1) {
        item.mane = murmurHash3("Horse_Link_Mane");
    }
}

void applyPatternDefaults(HorseItem& item) {
    if (item.iconPattern == 0) {
        item.iconPattern = murmurHash3("00");
    }
    if (item.iconEyeColor == 0) {
        item.iconEyeColor = murmurHash3("Black");
    }
}

void syncHorseTypeWithId(HorseItem& item) {
    if (item.id == "GameRomHorseZelda") {
        item.horseType = 3;
    } else if (item.id == "GameRomHorseEpona") {
        item.horseType = 4;
    } else if (item.id == "GameRomHorseSpPattern") {
        item.horseType = 8;
    } else if (item.id == "GameRomHorseGold") {
        item.horseType = 12;
    } else if (item.id == "GameRomHorse00L") {
        item.horseType = 2;
    } else if (item.id == "GameRomHorse01L") {
        item.horseType = 13;
    } else if (item.id == "GameRomHorseBone" || item.id == "GameRomHorseBone_AllDay") {
        item.horseType = 5;
    } else if (item.id == "GameRomHorseNushi") {
        item.horseType = 9;
    } else if (isNormalTamableBreed(item.id)) {
        item.horseType = 1;
    } else if (item.horseType == 0) {
        item.horseType = 1;
    }
}

void applyBreedOverrides(HorseItem& item) {
    syncHorseTypeWithId(item);

    if (item.id == "GameRomHorseZelda") {
        return;
    }

    if (item.id == "GameRomHorseEpona") {
        item.mane = murmurHash3("Horse_Link_Mane");
        item.saddle = murmurHash3("GameRomHorseSaddle_06");
        item.reins = murmurHash3("GameRomHorseReins_06");
        item.strength = 220;
        item.speed = 3;
        item.stamina = 4;
        item.pull = 2;
        item.colorType = 0;
        applyEponaPortraitColors(item);
        return;
    }

    if (item.id == "GameRomHorseSpPattern") {
        applySpottedPortraitColors(item);
        return;
    }

    if (item.id == "GameRomHorseGold") {
        item.strength = 100;
        item.speed = 2;
        item.stamina = 5;
        item.pull = 2;
        item.colorType = 1;
        clearPortraitColors(item);
        applyStableTackDefaults(item);
        applyPatternDefaults(item);
        return;
    }

    if (item.id == "GameRomHorse00L") {
        item.mane = murmurHash3("Horse_Link_Mane_00L");
        item.saddle = murmurHash3("GameRomHorseSaddle_00L");
        item.reins = murmurHash3("GameRomHorseReins_00L");
        item.strength = 100;
        item.speed = 1;
        item.stamina = 2;
        item.pull = 1;
        clearPortraitColors(item);
        applyPatternDefaults(item);
        return;
    }

    if (item.id == "GameRomHorse01L") {
        item.mane = murmurHash3("Horse_Link_Mane_01L");
        item.saddle = murmurHash3("GameRomHorseSaddle_00L");
        item.reins = murmurHash3("GameRomHorseReins_00L");
        item.strength = 100;
        item.speed = 1;
        item.stamina = 2;
        item.pull = 1;
        clearPortraitColors(item);
        applyPatternDefaults(item);
        return;
    }

    if (item.id == "GameRomHorse00S") {
        item.mane = murmurHash3("Horse_Link_Mane_00S");
        item.saddle = murmurHash3("GameRomHorseSaddle_00S");
        item.reins = murmurHash3("GameRomHorseReins_00S");
        item.strength = 100;
        item.speed = 1;
        item.stamina = 2;
        item.pull = 1;
        clearPortraitColors(item);
        applyPatternDefaults(item);
        return;
    }

    if (item.colorType == 0 && item.horseType == 1) {
        item.colorType = 1;
    }

    applyStableTackDefaults(item);
    applyPatternDefaults(item);
}

std::unordered_set<uint64_t> collectUsedUids(const std::vector<HorseItem>& horses, VariableStore* vars) {
    std::unordered_set<uint64_t> used;
    for (const auto& horse : horses) {
        if (horse.amiiboUid != 0) {
            used.insert(horse.amiiboUid);
        }
    }

    if (!vars) return used;

    try {
        const auto registry =
            vars->readUInt64Array(vars->resolveArrayPointer("Horse_UsedAmiiboUidHash"));
        for (uint64_t uid : registry) {
            if (uid != 0) {
                used.insert(uid);
            }
        }
    } catch (...) {
    }

    return used;
}

uint64_t generateUniqueUid(const std::vector<HorseItem>& horses, VariableStore* vars) {
    const auto used = collectUsedUids(horses, vars);
    uint64_t candidate = kMinGeneratedUid;
    for (uint64_t uid : used) {
        candidate = std::max(candidate, uid + 1);
    }
    while (used.count(candidate) != 0) {
        ++candidate;
    }
    return candidate;
}

}  // namespace

HorseItem createForId(const std::string& id, const std::vector<HorseItem>& owned, VariableStore* vars) {
    (void)owned;
    (void)vars;

    HorseItem item;
    item.id = id;
    applyCleanCreateDefaults(item);
    applyBreedOverrides(item);
    syncHorseTypeWithId(item);
    return item;
}

void finalizeForStableSave(HorseItem& item, const std::vector<HorseItem>& allHorses, VariableStore* vars) {
    syncHorseTypeWithId(item);

    if (item.colorType == 0 && item.horseType == 1) {
        item.colorType = 1;
    }

    if (item.amiiboUid == 0) {
        item.amiiboUid = generateUniqueUid(allHorses, vars);
    }

    applyStableTackDefaults(item);
    applyPatternDefaults(item);
    ensureSpawnMinimums(item);
    GameLimits::clampHorseItem(item);
}

void syncUsedUidRegistry(VariableStore* vars, const std::vector<HorseItem>& horses) {
    if (!vars) return;

    try {
        auto registry = vars->readUInt64Array(vars->resolveArrayPointer("Horse_UsedAmiiboUidHash"));
        std::unordered_set<uint64_t> known(registry.begin(), registry.end());
        bool changed = false;

        for (const auto& horse : horses) {
            if (horse.amiiboUid == 0 || known.count(horse.amiiboUid) != 0) {
                continue;
            }
            registry.push_back(horse.amiiboUid);
            known.insert(horse.amiiboUid);
            changed = true;
        }

        if (changed) {
            vars->writeUInt64Array(vars->resolveArrayPointer("Horse_UsedAmiiboUidHash"), registry);
        }
    } catch (...) {
    }
}

}  // namespace HorseDefaults
}  // namespace totk
