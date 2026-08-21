#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "save/item_enums.hpp"

namespace totk {

struct IntEnumOption {
    int32_t value = 0;
    std::string label;
};

class HorseEnums {
public:
    static const std::vector<ItemEnumOption>& manes();
    static const std::vector<ItemEnumOption>& saddles();
    static const std::vector<ItemEnumOption>& reins();
    static const std::vector<ItemEnumOption>& patterns();
    static const std::vector<ItemEnumOption>& eyeColors();

    static const std::vector<IntEnumOption>& horseTypes();
    static const std::vector<IntEnumOption>& statSpeeds();
    static const std::vector<IntEnumOption>& statStamina();
    static const std::vector<IntEnumOption>& statPull();

    static std::string labelForValue(const std::vector<ItemEnumOption>& options, uint32_t value);
    static std::string labelForValue(const std::vector<IntEnumOption>& options, int32_t value);
    static int indexForValue(const std::vector<IntEnumOption>& options, int32_t value);
    static std::vector<std::string> labelsFor(const std::vector<IntEnumOption>& options);
};

}  // namespace totk
