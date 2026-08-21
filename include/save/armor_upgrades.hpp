#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace totk {

class ArmorUpgrades {
public:
    static bool loadFromRomfs();

    static std::string baseId(const std::string& armorId);
    static int levelForId(const std::string& armorId);
    static int maxLevelForId(const std::string& armorId);
    static std::string idForLevel(const std::string& armorId, int level);
    static bool isDyeable(const std::string& armorId);
    static std::vector<std::string> levelLabelsFor(const std::string& armorId);
    static std::string levelLabel(int level);
};

}  // namespace totk
