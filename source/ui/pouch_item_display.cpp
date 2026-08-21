#include "ui/pouch_item_display.hpp"

#include "save/horse_enums.hpp"
#include "save/item_enums.hpp"
#include "save/save_editor.hpp"
#include "ui/item_database.hpp"

namespace totk {

namespace {

void applyFuseDisplayInfo(ItemDisplayInfo& info, ItemDatabase& db) {
    if (info.fuseId.empty()) return;

    info.fuseName = db.nameForId(info.fuseId);
    info.fuseIconPaths = db.iconPathCandidatesForId(info.fuseId);
}

}  // namespace

ItemDisplayInfo displayInfoForPouchItem(SaveEditor& editor, const std::string& category, int index) {
    ItemDisplayInfo info;
    auto& db = ItemDatabase::instance();

    if (index < 0) return info;

    if (category == "armors") {
        if (index >= static_cast<int>(editor.armors().size())) return info;
        const auto& item = editor.armors()[static_cast<size_t>(index)];
        info.id = item.id;
        info.name = db.displayNameForArmor(item.id);
        info.subtitle = item.id;
        info.iconPaths = db.iconPathCandidates("armors", item.id, item.dyeColor);
        return info;
    }

    if (category == "horses") {
        if (index >= static_cast<int>(editor.horses().size())) return info;
        const auto& item = editor.horses()[static_cast<size_t>(index)];
        info.id = item.id;
        info.name = item.name.empty() ? (item.id.empty() ? "Horse" : item.id) : item.name;
        info.badge = std::to_string(static_cast<int>(item.bond));
        const std::string breedLabel = db.nameForId(item.id);
        info.subtitle = (breedLabel.empty() ? item.id : breedLabel) + " · Bond " + info.badge;
        info.iconPaths = db.iconPathCandidates("horses", item.id);
        info.showTackPanel = true;

        const std::string saddleId = HorseEnums::labelForValue(HorseEnums::saddles(), item.saddle);
        if (!saddleId.empty() && saddleId != "None") {
            info.tackName = db.nameForId(saddleId);
            info.tackIconPaths = db.iconPathCandidatesForId(saddleId);
        } else {
            info.tackName = "None";
        }
        return info;
    }

    if (category == "weapons" || category == "bows" || category == "shields") {
        const auto it = editor.equipment().find(category);
        if (it == editor.equipment().end() || index >= static_cast<int>(it->second.size())) return info;
        const auto& item = it->second[static_cast<size_t>(index)];
        info.id = item.id;
        info.name = db.nameForId(item.id);
        info.badge = std::to_string(item.durability);
        info.subtitle = "Durability " + info.badge;
        if (category == "weapons" || category == "shields") {
            info.fuseId = item.fuseId;
            info.showFusePanel = true;
            applyFuseDisplayInfo(info, db);
        }
        info.iconPaths = db.iconPathCandidates(category, item.id);
        return info;
    }

    const auto it = editor.stackItems().find(category);
    if (it == editor.stackItems().end() || index >= static_cast<int>(it->second.size())) return info;
    const auto& item = it->second[static_cast<size_t>(index)];
    info.id = item.id;
    info.name = db.nameForId(item.id);
    info.badge = "x" + std::to_string(item.count);
    info.subtitle = "Quantity " + std::to_string(item.count);
    if (category == "food") {
        info.subtitle += " · Hearts " + std::to_string(item.hearts);
        const std::string effectLabel = ItemEnums::labelForValue(ItemEnums::foodEffects(), item.effect);
        if (!effectLabel.empty() && effectLabel != "None") {
            info.subtitle += " · " + effectLabel;
            if (item.effectLevel != 0) {
                info.subtitle += " Lv" + std::to_string(item.effectLevel);
            }
            if (item.effectDuration > 0) {
                info.subtitle += " " + std::to_string(item.effectDuration) + "s";
            }
        }
    }
    if (category == "food") {
        info.iconPaths = db.iconPathCandidates(category, item.id, 0, item.effect);
    } else {
        info.iconPaths = db.iconPathCandidates(category, item.id);
    }
    return info;
}

}  // namespace totk
