#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace totk {

struct ItemEnumOption {
    uint32_t value = 0;
    std::string label;
};

class ItemEnums {
public:
    static const std::vector<ItemEnumOption>& weaponModifiers();
    static const std::vector<ItemEnumOption>& bowModifiers();
    static const std::vector<ItemEnumOption>& shieldModifiers();
    static const std::vector<ItemEnumOption>& armorDyes();
    static const std::vector<ItemEnumOption>& foodEffects();

    static int indexForValue(const std::vector<ItemEnumOption>& options, uint32_t value);
    static std::string labelForValue(const std::vector<ItemEnumOption>& options, uint32_t value);
    static std::vector<std::string> labelsFor(const std::vector<ItemEnumOption>& options);
};

}  // namespace totk
