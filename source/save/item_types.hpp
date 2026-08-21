#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "save/variable_store.hpp"

namespace totk {

struct EquipmentItem {
    std::string category;
    std::string id;
    int32_t durability = 70;
    uint32_t modifier = 0;
    int32_t modifierValue = 0;
    std::string fuseId;
    int32_t fuseDurability = 0;
    int32_t extraDurability = 0;
    int32_t recordExtraDurability = 0;
};

struct StackItem {
    std::string category;
    std::string id;
    int32_t count = 1;
    int32_t getOrder = 0;
    int32_t useOrder = 0;
    int32_t hearts = 0;
    uint32_t effect = 0;
    int32_t effectLevel = 0;
    int32_t effectDuration = 0;
    int32_t price = 0;
    std::vector<std::string> ingredients;
};

struct ArmorItem {
    std::string id;
    uint32_t dyeColor = 0;
};

struct HorseItem {
    std::string id;
    std::string name;
    uint32_t mane = 0;
    uint32_t saddle = 0;
    uint32_t reins = 0;
    float bond = 0;
    bool bondChecked = false;
    int32_t strength = 100;
    int32_t speed = 1;
    int32_t stamina = 1;
    int32_t pull = 1;
    uint32_t horseType = 1;
    uint32_t colorType = 1;
    uint32_t footType = 0;
    uint64_t amiiboUid = 0;
    int32_t roomId = 0;

    uint32_t iconPattern = 0;
    uint32_t iconEyeColor = 0;
    uint32_t iconPrimaryColorRed = 0;
    uint32_t iconPrimaryColorGreen = 0;
    uint32_t iconPrimaryColorBlue = 0;
    uint32_t iconSecondaryColorRed = 0;
    uint32_t iconSecondaryColorGreen = 0;
    uint32_t iconSecondaryColorBlue = 0;
    uint32_t iconNoseColorRed = 0;
    uint32_t iconNoseColorGreen = 0;
    uint32_t iconNoseColorBlue = 0;
    uint32_t iconHairPrimaryColorRed = 0;
    uint32_t iconHairPrimaryColorGreen = 0;
    uint32_t iconHairPrimaryColorBlue = 0;
    uint32_t iconHairSecondaryColorRed = 0;
    uint32_t iconHairSecondaryColorGreen = 0;
    uint32_t iconHairSecondaryColorBlue = 0;
};

struct AutoBuilderSlot {
    int index = 0;
    int slotValue = -1;
    std::vector<uint8_t> blueprint;
    Vec3 cameraPos{};
    Vec3 cameraAt{};
    bool favorite = false;
};

}  // namespace totk
