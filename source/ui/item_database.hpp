#pragma once

#include <map>
#include <string>
#include <vector>

namespace totk {

struct ItemEntry {
    std::string id;
    std::string name;
};

class ItemDatabase {
public:
    static ItemDatabase& instance();

    bool loadFromRomfs();
    const std::vector<ItemEntry>& itemsForCategory(const std::string& category) const;
    const std::vector<ItemEntry>& pickerItemsForCategory(const std::string& category) const;
    const std::vector<ItemEntry>& fusibleItems() const;
    std::string nameForId(const std::string& id) const;
    std::string displayNameForArmor(const std::string& id) const;
    std::string categoryForId(const std::string& id) const;
    std::vector<std::string> iconPathCandidatesForId(const std::string& id, uint32_t dyeColor = 0) const;
    std::string iconPath(const std::string& category, const std::string& id, uint32_t dyeColor = 0,
                         uint32_t effect = 0) const;
    std::vector<std::string> iconPathCandidates(const std::string& category, const std::string& id,
                                                uint32_t dyeColor = 0, uint32_t effect = 0) const;
    bool hasPickerIcon(const std::string& category, const std::string& id) const;

private:
    ItemDatabase() = default;
    std::map<std::string, std::vector<ItemEntry>> byCategory_;
    std::map<std::string, std::string> namesById_;
    mutable std::vector<ItemEntry> fusibleItemsCache_;
    mutable bool fusibleItemsBuilt_ = false;
    mutable std::vector<ItemEntry> armorPickerCache_;
    mutable bool armorPickerBuilt_ = false;
};

}  // namespace totk
