#include "save/game_limits.hpp"

#include "save/armor_upgrades.hpp"
#include "save/murmur_hash.hpp"
#include "save/save_editor.hpp"

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace totk {

namespace {

template <typename T>
T clampValue(T value, T minValue, T maxValue) {
    return std::max(minValue, std::min(maxValue, value));
}

uint32_t snapToQuarterHearts(uint32_t quarters) {
    quarters = clampValue(quarters, GameLimits::kMaxLifeQuartersMin, GameLimits::kMaxLifeQuartersMax);
    const uint32_t remainder = quarters % 4;
    if (remainder == 0) return quarters;
    if (remainder >= 2 && quarters + (4 - remainder) <= GameLimits::kMaxLifeQuartersMax) {
        return quarters + (4 - remainder);
    }
    return quarters - remainder;
}

const std::vector<uint32_t>& validStaminaValues() {
    static const std::vector<uint32_t> kValues = {
        1148846080u, 1150681088u, 1152319488u, 1153957888u, 1155596288u, 1157234688u,
        1158250496u, 1159069696u, 1159888896u, 1160708096u, 1161527296u, 1342177279u,
    };
    return kValues;
}

const std::unordered_set<uint32_t>& validArmorDyeValues() {
    static const std::unordered_set<uint32_t> kValues = [] {
        std::unordered_set<uint32_t> values;
        const char* colors[] = {"None",   "Blue",        "Red",         "Yellow",      "White",
                                "Black",  "Purple",      "Green",       "LightBlue",   "Navy",
                                "Orange", "Pink",        "Crimson",     "LightYellow", "Brown",
                                "Gray"};
        for (const char* color : colors) {
            values.insert(murmurHash3(color));
        }
        return values;
    }();
    return kValues;
}

const std::unordered_map<std::string, int32_t>& maximumQuantityOverrides() {
    static const std::unordered_map<std::string, int32_t> kValues = {
        {"Item_Ore_L", 999999},
        {"Item_Ore_M", 999999},
        {"Energy_Material_01", 999999},
        {"Obj_WarpDLC", 3},
        {"Obj_KorokNuts", 1000},
        {"MinusRupee_00", 999999},
    };
    return kValues;
}

uint32_t hashModifier(const char* name) { return murmurHash3(name); }

bool isDurabilityModifier(uint32_t modifier) {
    static const uint32_t kDurabilityUp = hashModifier("DurabilityUp");
    static const uint32_t kDurabilityUpPlus = hashModifier("DurabilityUpPlus");
    return modifier == kDurabilityUp || modifier == kDurabilityUpPlus;
}

int32_t defaultEquipmentDurability() { return 70; }

int32_t equipmentMaximumDurability(const EquipmentItem& item) {
    int32_t maxDurability = defaultEquipmentDurability();
    if (isDurabilityModifier(item.modifier)) {
        const int32_t bonus = clampValue(item.modifierValue, 1, GameLimits::kModifierValueMax);
        maxDurability += bonus;
    }
    return maxDurability;
}

}  // namespace

int32_t GameLimits::equipmentMaximumDurability(const EquipmentItem& item) {
    int32_t maxDurability = 70;
    static const uint32_t kDurabilityUp = murmurHash3("DurabilityUp");
    static const uint32_t kDurabilityUpPlus = murmurHash3("DurabilityUpPlus");
    if (item.modifier == kDurabilityUp || item.modifier == kDurabilityUpPlus) {
        const int32_t bonus = std::max(1, std::min(item.modifierValue, kModifierValueMax));
        maxDurability += bonus;
    }
    return maxDurability;
}

uint32_t GameLimits::clampRupees(long value) {
    if (value < 0) return 0;
    if (value > static_cast<long>(kRupeesMax)) return kRupeesMax;
    return static_cast<uint32_t>(value);
}

uint32_t GameLimits::clampPonyPoints(long value) {
    if (value < 0) return 0;
    if (value > static_cast<long>(kPonyPointsMax)) return kPonyPointsMax;
    return static_cast<uint32_t>(value);
}

uint32_t GameLimits::clampMaxLifeQuarters(long value) {
    if (value < 0) return kMaxLifeQuartersMin;
    if (value > static_cast<long>(kMaxLifeQuartersMax)) {
        return kMaxLifeQuartersMax;
    }
    return snapToQuarterHearts(static_cast<uint32_t>(value));
}

uint32_t GameLimits::clampMaxStamina(long value) {
    const auto& values = validStaminaValues();
    if (value < 0) return values.front();
    if (value > 0xFFFFFFFFL) return values.back();

    const uint32_t raw = static_cast<uint32_t>(value);
    if (isValidStaminaValue(raw)) return raw;

    uint32_t closest = values.front();
    int64_t bestDistance = std::llabs(static_cast<int64_t>(raw) - static_cast<int64_t>(closest));
    for (uint32_t candidate : values) {
        const int64_t distance = std::llabs(static_cast<int64_t>(raw) - static_cast<int64_t>(candidate));
        if (distance < bestDistance) {
            bestDistance = distance;
            closest = candidate;
        }
    }
    return closest;
}

float GameLimits::clampBattery(long value) {
    long clamped = value;
    if (clamped < static_cast<long>(kBatteryMin)) clamped = kBatteryMin;
    if (clamped > static_cast<long>(kBatteryMax)) clamped = kBatteryMax;
    const long step = 1000;
    const long rounded = ((clamped + step / 2) / step) * step;
    return static_cast<float>(clampValue(rounded, static_cast<long>(kBatteryMin), static_cast<long>(kBatteryMax)));
}

uint32_t GameLimits::clampPouchWeapons(long value) {
    if (value < 0) return kPouchWeaponsMin;
    return static_cast<uint32_t>(clampValue(static_cast<uint32_t>(value), kPouchWeaponsMin, kPouchWeaponsMax));
}

uint32_t GameLimits::clampPouchBows(long value) {
    if (value < 0) return kPouchBowsMin;
    return static_cast<uint32_t>(clampValue(static_cast<uint32_t>(value), kPouchBowsMin, kPouchBowsMax));
}

uint32_t GameLimits::clampPouchShields(long value) {
    if (value < 0) return kPouchShieldsMin;
    return static_cast<uint32_t>(clampValue(static_cast<uint32_t>(value), kPouchShieldsMin, kPouchShieldsMax));
}

void GameLimits::clampPlayerStats(PlayerStats& stats) {
    stats.rupees = clampRupees(static_cast<long>(stats.rupees));
    stats.ponyPoints = clampPonyPoints(static_cast<long>(stats.ponyPoints));
    stats.maxHearts = clampMaxLifeQuarters(static_cast<long>(stats.maxHearts));
    stats.maxStamina = clampMaxStamina(static_cast<long>(stats.maxStamina));
    stats.maxBattery = clampBattery(static_cast<long>(stats.maxBattery));
    stats.pouchWeapons = clampPouchWeapons(static_cast<long>(stats.pouchWeapons));
    stats.pouchBows = clampPouchBows(static_cast<long>(stats.pouchBows));
    stats.pouchShields = clampPouchShields(static_cast<long>(stats.pouchShields));
    stats.mapPinCount = static_cast<int>(clampValue(stats.mapPinCount, 0, 300));
}

int32_t GameLimits::maximumStackQuantity(const std::string& itemId) {
    const auto& overrides = maximumQuantityOverrides();
    const auto it = overrides.find(itemId);
    if (it != overrides.end()) return it->second;
    return kStackQuantityDefaultMax;
}

bool GameLimits::isValidArmorDye(uint32_t dyeColor) { return validArmorDyeValues().count(dyeColor) > 0; }

bool GameLimits::isValidStaminaValue(uint32_t value) {
    for (uint32_t candidate : validStaminaValues()) {
        if (candidate == value) return true;
    }
    return false;
}

std::string GameLimits::staminaWheelLabel(uint32_t value) {
    switch (value) {
        case 1148846080u: return "1 wheel";
        case 1150681088u: return "1 wheel + 1/5";
        case 1152319488u: return "1 wheel + 2/5";
        case 1153957888u: return "1 wheel + 3/5";
        case 1155596288u: return "1 wheel + 4/5";
        case 1157234688u: return "2 wheels";
        case 1158250496u: return "2 wheels + 1/5";
        case 1159069696u: return "2 wheels + 2/5";
        case 1159888896u: return "2 wheels + 3/5";
        case 1160708096u: return "2 wheels + 4/5";
        case 1161527296u: return "3 wheels";
        case 1342177279u: return "Infinite (editor)";
        default: return {};
    }
}

void GameLimits::applyEquipmentFuse(EquipmentItem& item, const std::string& fuseId) {
    item.fuseId = fuseId;

    if (fuseId.empty()) {
        item.fuseDurability = 0;
        item.extraDurability = 0;
        item.recordExtraDurability = -1;
    } else if (item.category == "weapons") {
        item.extraDurability = 25;
        item.recordExtraDurability = 25;
        item.fuseDurability = equipmentMaximumDurability(item);
    } else {
        item.fuseDurability = equipmentMaximumDurability(item);
    }

    clampEquipmentItem(item);
}

void GameLimits::clampEquipmentItem(EquipmentItem& item) {
    static const uint32_t kNoneModifier = hashModifier("None");

    if (item.modifier == kNoneModifier) {
        item.modifierValue = 0;
    } else {
        item.modifierValue = clampValue(item.modifierValue, kModifierValueMin, kModifierValueMax);
        if (isDurabilityModifier(item.modifier) && item.modifierValue < 1) {
            item.modifierValue = 1;
        }
    }

    const int32_t maxDurability = equipmentMaximumDurability(item);
    if (item.id.empty()) {
        item.durability = 0;
    } else {
        item.durability = clampValue(item.durability, 1, maxDurability);
    }

    item.fuseDurability = clampValue(item.fuseDurability, -1, maxDurability);
    item.extraDurability = clampValue(item.extraDurability, -1, maxDurability);
    item.recordExtraDurability = clampValue(item.recordExtraDurability, -1, maxDurability);
}

void GameLimits::clampStackItem(StackItem& item) {
    const int32_t maxQuantity = maximumStackQuantity(item.id);
    const int32_t minQuantity = item.category == "food" ? 1 : -1;
    item.count = clampValue(item.count, minQuantity, maxQuantity);

    if (item.category == "food") {
        item.hearts = clampValue(item.hearts, kFoodHeartsMin, kFoodHeartsMax);
        item.effectLevel = clampValue(item.effectLevel, kFoodEffectLevelMin, kFoodEffectLevelMax);
        item.effectDuration = clampValue(item.effectDuration, kFoodEffectDurationMin, kFoodEffectDurationMax);
        item.price = clampValue(item.price, kFoodPriceMin, kFoodPriceMax);
    }
}

void GameLimits::clampArmorItem(ArmorItem& item) {
    if (!ArmorUpgrades::isDyeable(item.id)) {
        item.dyeColor = hashModifier("None");
    } else if (!isValidArmorDye(item.dyeColor)) {
        item.dyeColor = hashModifier("None");
    }
}

void GameLimits::clampHorseItem(HorseItem& item) {
    item.bond = clampValue(item.bond, 0.0f, 100.0f);
    item.strength = clampValue(item.strength, kHorseStrengthMin, kHorseStrengthMax);
    item.colorType = clampValue(item.colorType, 0u, kHorseColorTypeMax);
    item.footType = clampValue(item.footType, 0u, 1u);
    item.iconPrimaryColorRed = clampValue(item.iconPrimaryColorRed, 0u, kHorseIconColorMax);
    item.iconPrimaryColorGreen = clampValue(item.iconPrimaryColorGreen, 0u, kHorseIconColorMax);
    item.iconPrimaryColorBlue = clampValue(item.iconPrimaryColorBlue, 0u, kHorseIconColorMax);
    item.iconSecondaryColorRed = clampValue(item.iconSecondaryColorRed, 0u, kHorseIconColorMax);
    item.iconSecondaryColorGreen = clampValue(item.iconSecondaryColorGreen, 0u, kHorseIconColorMax);
    item.iconSecondaryColorBlue = clampValue(item.iconSecondaryColorBlue, 0u, kHorseIconColorMax);
    item.iconNoseColorRed = clampValue(item.iconNoseColorRed, 0u, kHorseIconColorMax);
    item.iconNoseColorGreen = clampValue(item.iconNoseColorGreen, 0u, kHorseIconColorMax);
    item.iconNoseColorBlue = clampValue(item.iconNoseColorBlue, 0u, kHorseIconColorMax);
    item.iconHairPrimaryColorRed = clampValue(item.iconHairPrimaryColorRed, 0u, kHorseIconColorMax);
    item.iconHairPrimaryColorGreen = clampValue(item.iconHairPrimaryColorGreen, 0u, kHorseIconColorMax);
    item.iconHairPrimaryColorBlue = clampValue(item.iconHairPrimaryColorBlue, 0u, kHorseIconColorMax);
    item.iconHairSecondaryColorRed = clampValue(item.iconHairSecondaryColorRed, 0u, kHorseIconColorMax);
    item.iconHairSecondaryColorGreen = clampValue(item.iconHairSecondaryColorGreen, 0u, kHorseIconColorMax);
    item.iconHairSecondaryColorBlue = clampValue(item.iconHairSecondaryColorBlue, 0u, kHorseIconColorMax);
}

void GameLimits::clampAllPouches(std::map<std::string, std::vector<EquipmentItem>>& equipment,
                                 std::map<std::string, std::vector<StackItem>>& stackItems,
                                 std::vector<ArmorItem>& armors,
                                 std::vector<HorseItem>& horses) {
    for (auto& [_, items] : equipment) {
        for (auto& item : items) {
            clampEquipmentItem(item);
        }
    }
    for (auto& [_, items] : stackItems) {
        for (auto& item : items) {
            clampStackItem(item);
        }
    }
    for (auto& item : armors) {
        clampArmorItem(item);
    }
    for (auto& item : horses) {
        clampHorseItem(item);
    }
}

}  // namespace totk
