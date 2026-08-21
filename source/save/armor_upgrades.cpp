#include "save/armor_upgrades.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <nlohmann/json.hpp>

#include "util/resource_loader.hpp"

namespace totk {

namespace {

std::vector<std::vector<std::string>> gUpgradeChains;
std::map<std::string, int> gChainIndexByNumber;
std::map<std::string, int> gLevelByNumber;
std::set<std::string> gDyeableNumbers;

std::optional<std::string> armorNumber(const std::string& armorId) {
    const auto first = armorId.find('_');
    if (first == std::string::npos) return std::nullopt;
    const auto second = armorId.find('_', first + 1);
    if (second == std::string::npos) return std::nullopt;
    return armorId.substr(first + 1, second - first - 1);
}

std::string replaceArmorNumber(const std::string& armorId, const std::string& newNumber) {
    const auto first = armorId.find('_');
    if (first == std::string::npos) return armorId;
    const auto second = armorId.find('_', first + 1);
    if (second == std::string::npos) return armorId;
    return armorId.substr(0, first + 1) + newNumber + armorId.substr(second);
}

const std::vector<std::string>* chainForNumber(const std::string& number) {
    const auto it = gChainIndexByNumber.find(number);
    if (it == gChainIndexByNumber.end()) return nullptr;
    if (it->second < 0 || static_cast<size_t>(it->second) >= gUpgradeChains.size()) return nullptr;
    return &gUpgradeChains[static_cast<size_t>(it->second)];
}

}  // namespace

bool ArmorUpgrades::loadFromRomfs() {
    gUpgradeChains.clear();
    gChainIndexByNumber.clear();
    gLevelByNumber.clear();
    gDyeableNumbers.clear();

    try {
        const auto content = totk::loadResourceText("data/armor_upgrades.json");
        if (content.empty()) return false;

        const auto json = nlohmann::json::parse(content);
        for (const auto& chainJson : json.at("upgrades")) {
            std::vector<std::string> chain;
            for (const auto& tier : chainJson) {
                chain.push_back(tier.get<std::string>());
            }
            if (chain.empty()) continue;

            const int chainIndex = static_cast<int>(gUpgradeChains.size());
            gUpgradeChains.push_back(chain);
            for (size_t level = 0; level < chain.size(); ++level) {
                gChainIndexByNumber[chain[level]] = chainIndex;
                gLevelByNumber[chain[level]] = static_cast<int>(level);
            }
        }

        for (const auto& number : json.at("dyeable")) {
            gDyeableNumbers.insert(number.get<std::string>());
        }

        return true;
    } catch (...) {
        return false;
    }
}

std::string ArmorUpgrades::baseId(const std::string& armorId) {
    const auto number = armorNumber(armorId);
    if (!number) return armorId;

    const auto* chain = chainForNumber(*number);
    if (!chain || chain->empty()) return armorId;
    return replaceArmorNumber(armorId, chain->front());
}

int ArmorUpgrades::levelForId(const std::string& armorId) {
    const auto number = armorNumber(armorId);
    if (!number) return 0;

    const auto it = gLevelByNumber.find(*number);
    return it == gLevelByNumber.end() ? 0 : it->second;
}

int ArmorUpgrades::maxLevelForId(const std::string& armorId) {
    const auto number = armorNumber(armorId);
    if (!number) return 0;

    const auto* chain = chainForNumber(*number);
    if (!chain || chain->empty()) return 0;
    return static_cast<int>(chain->size()) - 1;
}

std::string ArmorUpgrades::idForLevel(const std::string& armorId, int level) {
    const auto number = armorNumber(armorId);
    if (!number) return armorId;

    const auto* chain = chainForNumber(*number);
    if (!chain || chain->empty()) return armorId;

    const int clamped = std::max(0, std::min(level, static_cast<int>(chain->size()) - 1));
    return replaceArmorNumber(armorId, (*chain)[static_cast<size_t>(clamped)]);
}

bool ArmorUpgrades::isDyeable(const std::string& armorId) {
    const auto base = baseId(armorId);
    const auto number = armorNumber(base);
    if (!number) return false;
    return gDyeableNumbers.count(*number) > 0;
}

std::string ArmorUpgrades::levelLabel(int level) {
    switch (level) {
        case 0:
            return "Base";
        case 1:
            return "★";
        case 2:
            return "★★";
        case 3:
            return "★★★";
        case 4:
            return "★★★★";
        default:
            return "Level " + std::to_string(level);
    }
}

std::vector<std::string> ArmorUpgrades::levelLabelsFor(const std::string& armorId) {
    std::vector<std::string> labels;
    const int maxLevel = maxLevelForId(armorId);
    labels.reserve(static_cast<size_t>(maxLevel + 1));
    for (int level = 0; level <= maxLevel; ++level) {
        labels.push_back(levelLabel(level));
    }
    return labels;
}

}  // namespace totk
