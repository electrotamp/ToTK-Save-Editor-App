#pragma once

#include <functional>
#include <string>

#include <borealis.hpp>

#include "save/item_types.hpp"

namespace totk::ui {

class BoundedNumericCell;

class ItemEditorUi {
public:
    static void addSectionHeader(brls::Box* content, const std::string& title);
    static void addLabel(brls::Box* content, const std::string& text);

    static BoundedNumericCell* addBoundedField(
        brls::Box* content, const std::string& title, long value, long minValue, long maxValue,
        std::function<long(long)> clampFn, std::function<void(long)> onChange, const std::string& hint = "",
        bool allowNegative = false);

    static brls::DetailCell* addEquipmentEditor(brls::Box* content, EquipmentItem& item, bool withFuse, bool withModifier,
                                                std::function<void()> onFusePick = nullptr);
    static void addStackEditor(brls::Box* content, StackItem& item);
    static void addFoodEditor(brls::Box* content, StackItem& item);
    static void addArmorEditor(brls::Box* content, ArmorItem& item, std::function<void()> onChanged = nullptr);
    static void addHorseEditor(brls::Box* content, HorseItem& item, std::function<void()> onChanged = nullptr);
};

}  // namespace totk::ui
