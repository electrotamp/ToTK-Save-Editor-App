#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "save/item_types.hpp"

namespace totk {

struct PlayerStats;

struct GameLimits {
    static constexpr uint32_t kRupeesMax = 999999;
    static constexpr uint32_t kPonyPointsMax = 999999;
    static constexpr uint32_t kMaxLifeQuartersMin = 4;
    static constexpr uint32_t kMaxLifeQuartersMax = 160;
    static constexpr uint32_t kBatteryMin = 3000;
    static constexpr uint32_t kBatteryMax = 48000;
    static constexpr uint32_t kPouchWeaponsMin = 9;
    static constexpr uint32_t kPouchWeaponsMax = 20;
    static constexpr uint32_t kPouchBowsMin = 5;
    static constexpr uint32_t kPouchBowsMax = 14;
    static constexpr uint32_t kPouchShieldsMin = 4;
    static constexpr uint32_t kPouchShieldsMax = 20;
    static constexpr int32_t kStackQuantityDefaultMax = 999;
    static constexpr int32_t kModifierValueMin = -1;
    static constexpr int32_t kModifierValueMax = 2100000000;
    static constexpr int32_t kFoodHeartsMin = -1;
    static constexpr int32_t kFoodHeartsMax = 160;
    static constexpr int32_t kFoodEffectLevelMin = -1;
    static constexpr int32_t kFoodEffectLevelMax = 250;
    static constexpr int32_t kFoodEffectDurationMin = -1;
    static constexpr int32_t kFoodEffectDurationMax = 59999;
    static constexpr int32_t kFoodPriceMin = 1;
    static constexpr int32_t kFoodPriceMax = 999999;
    static constexpr int32_t kHorseStrengthMin = 100;
    static constexpr int32_t kHorseStrengthMax = 350;
    static constexpr uint32_t kHorseColorTypeMax = 40;
    static constexpr uint32_t kHorseIconColorMax = 255;

    static uint32_t clampRupees(long value);
    static uint32_t clampPonyPoints(long value);
    static uint32_t clampMaxLifeQuarters(long value);
    static uint32_t clampMaxStamina(long value);
    static float clampBattery(long value);
    static uint32_t clampPouchWeapons(long value);
    static uint32_t clampPouchBows(long value);
    static uint32_t clampPouchShields(long value);

    static void clampPlayerStats(PlayerStats& stats);
    static void clampEquipmentItem(EquipmentItem& item);
    static void applyEquipmentFuse(EquipmentItem& item, const std::string& fuseId);
    static void clampStackItem(StackItem& item);
    static void clampArmorItem(ArmorItem& item);
    static void clampHorseItem(HorseItem& item);

    static void clampAllPouches(std::map<std::string, std::vector<EquipmentItem>>& equipment,
                                std::map<std::string, std::vector<StackItem>>& stackItems,
                                std::vector<ArmorItem>& armors,
                                std::vector<HorseItem>& horses);

    static int32_t equipmentMaximumDurability(const EquipmentItem& item);
    static int32_t maximumStackQuantity(const std::string& itemId);
    static bool isValidArmorDye(uint32_t dyeColor);
    static bool isValidStaminaValue(uint32_t value);
    static std::string staminaWheelLabel(uint32_t value);
};

}  // namespace totk
