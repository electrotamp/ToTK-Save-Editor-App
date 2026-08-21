#include "ui/item_database.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <nlohmann/json.hpp>

#include "save/armor_upgrades.hpp"
#include "save/item_enums.hpp"
#include "save/murmur_hash.hpp"
#include "util/icon_texture_cache.hpp"
#include "util/resource_loader.hpp"

namespace totk {

namespace {

const std::map<std::string, std::string> kDecayedToPristineWeapon = {
    {"Weapon_Sword_106", "Weapon_Sword_001"},   {"Weapon_Sword_112", "Weapon_Sword_002"},
    {"Weapon_Sword_113", "Weapon_Sword_003"},   {"Weapon_Sword_124", "Weapon_Sword_024"},
    {"Weapon_Sword_125", "Weapon_Sword_025"},   {"Weapon_Sword_127", "Weapon_Sword_027"},
    {"Weapon_Sword_129", "Weapon_Sword_029"},   {"Weapon_Sword_131", "Weapon_Sword_031"},
    {"Weapon_Sword_114", "Weapon_Sword_041"},   {"Weapon_Sword_147", "Weapon_Sword_047"},
    {"Weapon_Sword_168", "Weapon_Sword_103"},   {"Weapon_Lsword_106", "Weapon_Lsword_001"},
    {"Weapon_Lsword_112", "Weapon_Lsword_002"}, {"Weapon_Lsword_113", "Weapon_Lsword_003"},
    {"Weapon_Lsword_124", "Weapon_Lsword_024"}, {"Weapon_Lsword_127", "Weapon_Lsword_027"},
    {"Weapon_Lsword_129", "Weapon_Lsword_029"}, {"Weapon_Lsword_136", "Weapon_Lsword_036"},
    {"Weapon_Lsword_114", "Weapon_Lsword_041"}, {"Weapon_Lsword_147", "Weapon_Lsword_047"},
    {"Weapon_Lsword_168", "Weapon_Lsword_103"}, {"Weapon_Lsword_174", "Weapon_Lsword_051"},
    {"Weapon_Spear_106", "Weapon_Spear_001"},   {"Weapon_Spear_112", "Weapon_Spear_002"},
    {"Weapon_Spear_113", "Weapon_Spear_003"},   {"Weapon_Spear_124", "Weapon_Spear_024"},
    {"Weapon_Spear_125", "Weapon_Spear_025"},   {"Weapon_Spear_127", "Weapon_Spear_027"},
    {"Weapon_Spear_129", "Weapon_Spear_029"},   {"Weapon_Spear_173", "Weapon_Spear_030"},
    {"Weapon_Spear_132", "Weapon_Spear_032"},   {"Weapon_Spear_147", "Weapon_Spear_047"},
    {"Weapon_Spear_168", "Weapon_Spear_103"},
};

std::string stripDecayedSuffix(const std::string& id) {
    const auto pos = id.find(" (decayed)");
    if (pos != std::string::npos) return id.substr(0, pos);
    return id;
}

constexpr const char* kElixirEffectNames[] = {
    "AllSpeed",      "AttackUp",        "DefenseUp",       "ExStaminaMaxUp", "LifeMaxUp",
    "LightEmission", "NotSlippy",       "QuietnessUp",     "ResistBurn",     "ResistCold",
    "ResistElectric", "ResistHot",      "StaminaRecover",
};

bool sharesFoodMaterialIconFolders(const std::string& id) {
    if (id.rfind("Item_Cook_", 0) == 0 || id.rfind("Item_Fruit_", 0) == 0 || id.rfind("Item_Meat_", 0) == 0 ||
        id.rfind("Item_Fish_", 0) == 0 || id.rfind("Item_Mushroom", 0) == 0 || id.rfind("Item_Roast", 0) == 0 ||
        id.rfind("Item_Chilled", 0) == 0 || id.rfind("Item_Boiled", 0) == 0) {
        return true;
    }
    return false;
}

std::string elixirEffectSuffix(uint32_t effect) {
    if (effect == 0) return {};
    for (const char* name : kElixirEffectNames) {
        if (murmurHash3(name) == effect) return name;
    }
    return {};
}

std::string armorDyeSuffix(uint32_t dyeColor) {
    if (dyeColor == 0) return {};
    static const uint32_t kNone = murmurHash3("None");
    if (dyeColor == kNone) return {};
    for (const auto& option : ItemEnums::armorDyes()) {
        if (option.value == dyeColor && option.label != "None") {
            return option.label;
        }
    }
    return {};
}

std::map<std::string, std::string> gArmorBaseById;

std::string armorIconBaseId(const std::string& armorId) {
    const auto it = gArmorBaseById.find(armorId);
    return it == gArmorBaseById.end() ? armorId : it->second;
}

std::string stripStarSuffix(const std::string& name) {
    const auto pos = name.find(" ★");
    return pos == std::string::npos ? name : name.substr(0, pos);
}

}  // namespace

ItemDatabase& ItemDatabase::instance() {
    static ItemDatabase db;
    return db;
}

bool ItemDatabase::loadFromRomfs() {
    try {
        const auto content = totk::loadResourceText("data/items.json");
        if (content.empty()) return false;
        const auto json = nlohmann::json::parse(content);
        byCategory_.clear();
        namesById_.clear();
        for (auto it = json.begin(); it != json.end(); ++it) {
            std::vector<ItemEntry> entries;
            for (const auto& item : it.value()) {
                ItemEntry entry{item.value("id", ""), item.value("name", "")};
                entries.push_back(entry);
                namesById_[entry.id] = entry.name;
            }
            byCategory_[it.key()] = std::move(entries);
        }

        gArmorBaseById.clear();
        const auto armorBases = totk::loadResourceText("data/armor_bases.json");
        if (!armorBases.empty()) {
            const auto basesJson = nlohmann::json::parse(armorBases);
            for (auto it = basesJson.begin(); it != basesJson.end(); ++it) {
                gArmorBaseById[it.key()] = it.value().get<std::string>();
            }
        }

        ArmorUpgrades::loadFromRomfs();

        return true;
    } catch (...) {
        return false;
    }
}

const std::vector<ItemEntry>& ItemDatabase::itemsForCategory(const std::string& category) const {
    static const std::vector<ItemEntry> empty;
    const auto it = byCategory_.find(category);
    return it == byCategory_.end() ? empty : it->second;
}

const std::vector<ItemEntry>& ItemDatabase::pickerItemsForCategory(const std::string& category) const {
    if (category != "armors") {
        return itemsForCategory(category);
    }

    if (armorPickerBuilt_) {
        return armorPickerCache_;
    }

    armorPickerBuilt_ = true;
    armorPickerCache_.clear();

    std::set<std::string> seenBases;
    for (const auto& entry : itemsForCategory("armors")) {
        if (entry.id.empty()) continue;

        const std::string base = ArmorUpgrades::baseId(entry.id);
        if (!seenBases.insert(base).second) continue;

        ItemEntry pickerEntry;
        pickerEntry.id = base;
        pickerEntry.name = displayNameForArmor(base);
        armorPickerCache_.push_back(std::move(pickerEntry));
    }

    std::sort(armorPickerCache_.begin(), armorPickerCache_.end(),
              [](const ItemEntry& left, const ItemEntry& right) { return left.name < right.name; });

    return armorPickerCache_;
}

namespace {

// Ids that sit in the weapons/shields/materials arrays purely as save-data
// bookkeeping (boss combat props, quest/cutscene objects, house-blueprint
// unlocks, internal actor references) — never selectable as a fuse material
// in the actual game, unlike everything else in those categories which is.
bool isNonFusableJunkId(const std::string& id) {
    static const std::set<std::string> exact = {
        "Weapon_DungeonBossZonau", "Weapon_Goron_Knuckle", "Weapon_RaumiGolem_Left", "Weapon_RaumiGolem_Right",
        "Weapon_DungeonBossZonau_Front", "Weapon_RaumiGolem_Back",
        "AssassinRockBall", "OctObj_Stone_TBox", "Obj_Cushion", "Animal_Insect_NA",
    };
    if (exact.count(id)) return true;
    if (id.rfind("Npc_Assassin_", 0) == 0) return true;
    if (id.rfind("Obj_LinkHouse_", 0) == 0) return true;
    if (id.rfind("DgnObj_", 0) == 0) return true;
    return false;
}

}  // namespace

const std::vector<ItemEntry>& ItemDatabase::fusibleItems() const {
    if (fusibleItemsBuilt_) {
        return fusibleItemsCache_;
    }

    fusibleItemsBuilt_ = true;
    fusibleItemsCache_.clear();
    fusibleItemsCache_.push_back({"", "None (no fusion)"});

    std::map<std::string, ItemEntry> unique;
    const auto addCategory = [this, &unique](const std::string& category) {
        for (const auto& entry : itemsForCategory(category)) {
            if (!entry.id.empty() && !isNonFusableJunkId(entry.id)) {
                unique[entry.id] = entry;
            }
        }
    };

    addCategory("weapons");
    addCategory("bows");
    addCategory("shields");
    addCategory("materials");
    for (const auto& entry : itemsForCategory("food")) {
        if (entry.id.find("_Roast") != std::string::npos || entry.id.find("_Chilled") != std::string::npos) {
            unique[entry.id] = entry;
        }
    }

    for (const auto& [id, entry] : unique) {
        (void)id;
        fusibleItemsCache_.push_back(entry);
    }

    if (fusibleItemsCache_.size() > 1) {
        std::sort(fusibleItemsCache_.begin() + 1, fusibleItemsCache_.end(),
                  [](const ItemEntry& left, const ItemEntry& right) { return left.name < right.name; });
    }

    return fusibleItemsCache_;
}

std::string ItemDatabase::nameForId(const std::string& id) const {
    const auto it = namesById_.find(id);
    if (it == namesById_.end() || it->second.empty()) return id;
    return it->second;
}

std::string ItemDatabase::displayNameForArmor(const std::string& id) const {
    std::string name = stripStarSuffix(nameForId(id));
    if (name.empty() || name == id) {
        name = stripStarSuffix(nameForId(ArmorUpgrades::baseId(id)));
    }
    if (name.empty()) name = id;

    const int level = ArmorUpgrades::levelForId(id);
    if (level > 0) {
        name += " " + ArmorUpgrades::levelLabel(level);
    }
    return name;
}

std::string ItemDatabase::categoryForId(const std::string& id) const {
    if (id.empty()) return {};

    if (id.rfind("Weapon_Bow_", 0) == 0) return "bows";
    if (id == "NormalArrow") return "arrows";
    if (id.rfind("Weapon_Shield_", 0) == 0) return "shields";
    if (id.rfind("Weapon_", 0) == 0) return "weapons";
    if (id.rfind("Arrow_", 0) == 0) return "arrows";
    if (id.rfind("Item_Cook_", 0) == 0 || id.rfind("Item_Fruit_", 0) == 0 || id.rfind("Item_Meat_", 0) == 0 ||
        id.rfind("Item_Fish_", 0) == 0) {
        return "food";
    }
    if (id.rfind("Obj_Zonau_", 0) == 0 || id.rfind("SpObj_", 0) == 0) return "devices";
    if (id.rfind("AsbObj_", 0) == 0) return "materials";
    if (id.rfind("Armor_", 0) == 0) return "armors";
    if (id.rfind("GameRomHorse", 0) == 0) return "horses";
    if (id.rfind("Horse_", 0) == 0) return "horses";
    if (id.rfind("Key_", 0) == 0 || id.rfind("Parasail_", 0) == 0 || id.rfind("Npc_", 0) == 0 ||
        id.rfind("Demo", 0) == 0) {
        return "key";
    }
    if (id.rfind("Obj_", 0) == 0 || id.rfind("Animal_", 0) == 0 || id.rfind("Item_", 0) == 0) {
        return "materials";
    }

    return {};
}

std::vector<std::string> ItemDatabase::iconPathCandidatesForId(const std::string& id, uint32_t dyeColor) const {
    const std::string category = categoryForId(id);
    if (!category.empty()) {
        return iconPathCandidates(category, id, dyeColor);
    }

    std::vector<std::string> paths;
    for (const char* fallbackCategory : {"materials", "food", "devices", "weapons", "bows", "shields", "key"}) {
        const auto candidates = iconPathCandidates(fallbackCategory, id, dyeColor);
        paths.insert(paths.end(), candidates.begin(), candidates.end());
    }
    return paths;
}

std::string ItemDatabase::iconPath(const std::string& category, const std::string& id, uint32_t dyeColor,
                                   uint32_t effect) const {
    const auto candidates = iconPathCandidates(category, id, dyeColor, effect);
    return candidates.empty() ? std::string{} : candidates.front();
}

std::vector<std::string> ItemDatabase::iconPathCandidates(const std::string& category, const std::string& id,
                                                          uint32_t dyeColor, uint32_t effect) const {
    std::vector<std::string> paths;
    if (id.empty()) return paths;

    auto pushUnique = [&](const std::string& path) {
        if (std::find(paths.begin(), paths.end(), path) == paths.end()) paths.push_back(path);
    };

    std::string baseId = stripDecayedSuffix(id);

    auto pushCategoryIcon = [&](const std::string& iconCategory, const std::string& fileName) {
        pushUnique("assets/item_icons/" + iconCategory + "/" + fileName);
    };

    if (category == "armors") {
        const std::string iconBaseId = armorIconBaseId(baseId);
        const std::string dyeSuffix = armorDyeSuffix(dyeColor);
        if (!dyeSuffix.empty()) {
            pushCategoryIcon("armors/dye", iconBaseId + "_" + dyeSuffix + ".png");
        }
        pushCategoryIcon(category, iconBaseId + ".png");
        return paths;
    }

    if (category == "food" && baseId == "Item_Cook_C_17") {
        const std::string effectSuffix = elixirEffectSuffix(effect);
        if (!effectSuffix.empty()) {
            pushCategoryIcon(category, "Item_Cook_C_17_" + effectSuffix + ".png");
        }
    }

    pushCategoryIcon(category, id + ".png");
    if (baseId != id) {
        pushCategoryIcon(category, baseId + ".png");
    }

    if ((category == "food" || category == "materials") && sharesFoodMaterialIconFolders(baseId)) {
        const std::string alternateCategory = category == "food" ? "materials" : "food";
        pushCategoryIcon(alternateCategory, baseId + ".png");
        if (baseId != id) {
            pushCategoryIcon(alternateCategory, id + ".png");
        }
    }

    const auto pristineIt = kDecayedToPristineWeapon.find(baseId);
    if (pristineIt != kDecayedToPristineWeapon.end()) {
        pushCategoryIcon(category, pristineIt->second + ".png");
    }

    return paths;
}

bool ItemDatabase::hasPickerIcon(const std::string& category, const std::string& id) const {
    if (id.empty()) return true;
    for (const auto& path : iconPathCandidates(category, id)) {
        if (IconTextureCache::instance().hasAsset(path)) {
            return true;
        }
    }
    return false;
}

}  // namespace totk
