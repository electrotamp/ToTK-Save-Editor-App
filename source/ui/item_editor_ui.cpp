#include "ui/item_editor_ui.hpp"

#include <algorithm>

#include "save/armor_upgrades.hpp"
#include "save/game_limits.hpp"
#include "save/horse_enums.hpp"
#include "save/item_enums.hpp"
#include "save/murmur_hash.hpp"
#include "ui/bounded_numeric_cell.hpp"
#include "ui/item_database.hpp"

#include <borealis/core/touch/tap_gesture.hpp>

namespace totk::ui {

void ItemEditorUi::addSectionHeader(brls::Box* content, const std::string& title) {
    auto* header = new brls::Header();
    header->setTitle(title);
    header->setMarginTop(12);
    content->addView(header);
}

void ItemEditorUi::addLabel(brls::Box* content, const std::string& text) {
    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(18);
    label->setMarginBottom(4);
    content->addView(label);
}

BoundedNumericCell* ItemEditorUi::addBoundedField(brls::Box* content, const std::string& title, long value,
                                                  long minValue, long maxValue, std::function<long(long)> clampFn,
                                                  std::function<void(long)> onChange, const std::string& hint,
                                                  bool allowNegative) {
    auto* cell = new BoundedNumericCell();
    cell->init(title, value, minValue, maxValue, std::move(clampFn),
               [onChange = std::move(onChange)](long v) { onChange(v); }, hint, allowNegative);
    content->addView(cell);
    return cell;
}

brls::DetailCell* ItemEditorUi::addEquipmentEditor(brls::Box* content, EquipmentItem& item, bool withFuse,
                                                   bool withModifier, std::function<void()> onFusePick) {
    const auto clampAndStore = [&item](long value) {
        item.durability = static_cast<int32_t>(value);
        totk::GameLimits::clampEquipmentItem(item);
        return static_cast<long>(item.durability);
    };

    const int32_t maxDurability = totk::GameLimits::equipmentMaximumDurability(item);
    addBoundedField(content, "Durability", item.durability, 1, maxDurability, clampAndStore,
                    [&item](long value) { item.durability = static_cast<int32_t>(value); totk::GameLimits::clampEquipmentItem(item); });

    if (withModifier) {
        const std::vector<ItemEnumOption>& options = item.category == "bows"   ? ItemEnums::bowModifiers()
                                                     : item.category == "shields" ? ItemEnums::shieldModifiers()
                                                                                  : ItemEnums::weaponModifiers();
        auto* modifierCell = new brls::SelectorCell();
        modifierCell->init(
            "Modifier", ItemEnums::labelsFor(options), ItemEnums::indexForValue(options, item.modifier),
            [&item, &options](int selected) {
                item.modifier = options[static_cast<size_t>(selected)].value;
                if (item.modifier == murmurHash3("None")) {
                    item.modifierValue = 0;
                }
                totk::GameLimits::clampEquipmentItem(item);
            });
        // brls::SelectorCell registers its click action for gamepad A only —
        // touch needs an explicit tap gesture (see BoundedNumericCell's ctor).
        modifierCell->addGestureRecognizer(new brls::TapGestureRecognizer(modifierCell));
        content->addView(modifierCell);

        addBoundedField(
            content, "Modifier Value", item.modifierValue, totk::GameLimits::kModifierValueMin,
            totk::GameLimits::kModifierValueMax,
            [&item](long value) {
                item.modifierValue = static_cast<int32_t>(value);
                totk::GameLimits::clampEquipmentItem(item);
                return static_cast<long>(item.modifierValue);
            },
            [&item](long value) {
                item.modifierValue = static_cast<int32_t>(value);
                totk::GameLimits::clampEquipmentItem(item);
            },
            "", true);
    }

    if (withFuse) {
        auto* fuseCell = new brls::DetailCell();
        fuseCell->setText("Fused Item");
        const std::string fuseName = item.fuseId.empty() ? std::string() : ItemDatabase::instance().nameForId(item.fuseId);
        fuseCell->setDetailText(fuseName.empty() ? "Choose material" : fuseName);
        if (onFusePick) {
            fuseCell->registerClickAction([onFusePick](brls::View*) {
                onFusePick();
                return true;
            });
            fuseCell->addGestureRecognizer(new brls::TapGestureRecognizer(fuseCell));
        }
        content->addView(fuseCell);
        return fuseCell;
    }

    return nullptr;
}

void ItemEditorUi::addStackEditor(brls::Box* content, StackItem& item) {
    const int32_t maxQuantity = totk::GameLimits::maximumStackQuantity(item.id);
    const long minQuantity = item.category == "food" ? 1 : -1;
    addBoundedField(
        content, "Quantity", item.count, minQuantity, maxQuantity,
        [&item](long value) {
            item.count = static_cast<int32_t>(value);
            totk::GameLimits::clampStackItem(item);
            return static_cast<long>(item.count);
        },
        [&item](long value) {
            item.count = static_cast<int32_t>(value);
            totk::GameLimits::clampStackItem(item);
        },
        "", minQuantity < 0);
}

void ItemEditorUi::addFoodEditor(brls::Box* content, StackItem& item) {
    addStackEditor(content, item);

    addSectionHeader(content, "Effect");
    const std::vector<ItemEnumOption>& effects = ItemEnums::foodEffects();
    auto* effectCell = new brls::SelectorCell();
    effectCell->init(
        "Effect Type", ItemEnums::labelsFor(effects), ItemEnums::indexForValue(effects, item.effect),
        [&item, &effects](int selected) {
            item.effect = effects[static_cast<size_t>(selected)].value;
            if (item.effect == murmurHash3("None")) {
                item.effectLevel = 0;
                item.effectDuration = 0;
            }
            totk::GameLimits::clampStackItem(item);
        });
    effectCell->addGestureRecognizer(new brls::TapGestureRecognizer(effectCell));
    content->addView(effectCell);

    addBoundedField(
        content, "Effect Level", item.effectLevel, totk::GameLimits::kFoodEffectLevelMin,
        totk::GameLimits::kFoodEffectLevelMax,
        [&item](long value) {
            item.effectLevel = static_cast<int32_t>(value);
            totk::GameLimits::clampStackItem(item);
            return static_cast<long>(item.effectLevel);
        },
        [&item](long value) {
            item.effectLevel = static_cast<int32_t>(value);
            totk::GameLimits::clampStackItem(item);
        },
        "Potency multiplier", true);
    addBoundedField(
        content, "Effect Duration", item.effectDuration, totk::GameLimits::kFoodEffectDurationMin,
        totk::GameLimits::kFoodEffectDurationMax,
        [&item](long value) {
            item.effectDuration = static_cast<int32_t>(value);
            totk::GameLimits::clampStackItem(item);
            return static_cast<long>(item.effectDuration);
        },
        [&item](long value) {
            item.effectDuration = static_cast<int32_t>(value);
            totk::GameLimits::clampStackItem(item);
        },
        "Seconds", true);

    addSectionHeader(content, "Stats");
    addBoundedField(
        content, "Hearts Healed", item.hearts, totk::GameLimits::kFoodHeartsMin, totk::GameLimits::kFoodHeartsMax,
        [&item](long value) {
            item.hearts = static_cast<int32_t>(value);
            totk::GameLimits::clampStackItem(item);
            return static_cast<long>(item.hearts);
        },
        [&item](long value) {
            item.hearts = static_cast<int32_t>(value);
            totk::GameLimits::clampStackItem(item);
        },
        "", true);
    addBoundedField(
        content, "Price", item.price, totk::GameLimits::kFoodPriceMin, totk::GameLimits::kFoodPriceMax,
        [&item](long value) {
            item.price = static_cast<int32_t>(value);
            totk::GameLimits::clampStackItem(item);
            return static_cast<long>(item.price);
        },
        [&item](long value) {
            item.price = static_cast<int32_t>(value);
            totk::GameLimits::clampStackItem(item);
        });
}

void ItemEditorUi::addArmorEditor(brls::Box* content, ArmorItem& item, std::function<void()> onChanged) {
    const auto notify = [onChanged]() {
        if (onChanged) onChanged();
    };

    auto* notDyeableLabel = new brls::Label();
    notDyeableLabel->setText("This armor cannot be dyed.");
    notDyeableLabel->setFontSize(18);
    notDyeableLabel->setMarginBottom(4);

    const std::vector<ItemEnumOption>& dyes = ItemEnums::armorDyes();
    auto* dyeCell = new brls::SelectorCell();
    dyeCell->init(
        "Dye Color", ItemEnums::labelsFor(dyes), ItemEnums::indexForValue(dyes, item.dyeColor),
        [&item, &dyes, notify](int selected) {
            item.dyeColor = dyes[static_cast<size_t>(selected)].value;
            totk::GameLimits::clampArmorItem(item);
            notify();
        });

    const auto updateDyeUi = [dyeCell, notDyeableLabel, &item]() {
        const bool dyeable = ArmorUpgrades::isDyeable(item.id);
        dyeCell->setVisibility(dyeable ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        notDyeableLabel->setVisibility(dyeable ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    };

    const int currentLevel = ArmorUpgrades::levelForId(item.id);
    const int maxLevel = ArmorUpgrades::maxLevelForId(item.id);
    if (maxLevel > 0) {
        const std::vector<std::string> levelLabels = ArmorUpgrades::levelLabelsFor(item.id);
        auto* levelCell = new brls::SelectorCell();
        levelCell->init(
            "Upgrade Level", levelLabels, std::min(currentLevel, maxLevel),
            [&item, maxLevel, notify, updateDyeUi](int selected) {
                const int level = std::max(0, std::min(selected, maxLevel));
                item.id = ArmorUpgrades::idForLevel(item.id, level);
                if (!ArmorUpgrades::isDyeable(item.id)) {
                    item.dyeColor = murmurHash3("None");
                }
                totk::GameLimits::clampArmorItem(item);
                updateDyeUi();
                notify();
            });
        levelCell->addGestureRecognizer(new brls::TapGestureRecognizer(levelCell));
        content->addView(levelCell);
    } else {
        addLabel(content, "Single-tier armor (no upgrades)");
    }

    dyeCell->addGestureRecognizer(new brls::TapGestureRecognizer(dyeCell));
    updateDyeUi();
    content->addView(notDyeableLabel);
    content->addView(dyeCell);
}

void ItemEditorUi::addHorseEditor(brls::Box* content, HorseItem& item, std::function<void()> onChanged) {
    const auto notify = [onChanged]() {
        if (onChanged) onChanged();
    };

    addSectionHeader(content, "Identity");
    if (!item.id.empty()) {
        addLabel(content, totk::ItemDatabase::instance().nameForId(item.id));
    }

    auto* nameCell = new brls::InputCell();
    nameCell->init(
        "Name", item.name,
        [&item, notify](const std::string& value) {
            item.name = value;
            totk::GameLimits::clampHorseItem(item);
            notify();
        },
        "Horse name", "Up to 9 characters", 9);
    nameCell->addGestureRecognizer(new brls::TapGestureRecognizer(nameCell));
    content->addView(nameCell);

    addSectionHeader(content, "Stats");
    addBoundedField(
        content, "Bond", static_cast<long>(item.bond), 0, 100,
        [&item](long value) {
            item.bond = static_cast<float>(std::clamp(value, 0L, 100L));
            totk::GameLimits::clampHorseItem(item);
            return static_cast<long>(item.bond);
        },
        [&item, notify](long value) {
            item.bond = static_cast<float>(value);
            totk::GameLimits::clampHorseItem(item);
            notify();
        });

    addBoundedField(
        content, "Strength", item.strength, totk::GameLimits::kHorseStrengthMin, totk::GameLimits::kHorseStrengthMax,
        [&item](long value) {
            item.strength = static_cast<int32_t>(value);
            totk::GameLimits::clampHorseItem(item);
            return static_cast<long>(item.strength);
        },
        [&item, notify](long value) {
            item.strength = static_cast<int32_t>(value);
            totk::GameLimits::clampHorseItem(item);
            notify();
        });

    const auto& speedOptions = HorseEnums::statSpeeds();
    auto* speedCell = new brls::SelectorCell();
    speedCell->init(
        "Speed", HorseEnums::labelsFor(speedOptions), HorseEnums::indexForValue(speedOptions, item.speed),
        [&item, &speedOptions, notify](int selected) {
            item.speed = speedOptions[static_cast<size_t>(selected)].value;
            notify();
        });
    speedCell->addGestureRecognizer(new brls::TapGestureRecognizer(speedCell));
    content->addView(speedCell);

    const auto& staminaOptions = HorseEnums::statStamina();
    auto* staminaCell = new brls::SelectorCell();
    staminaCell->init(
        "Stamina", HorseEnums::labelsFor(staminaOptions), HorseEnums::indexForValue(staminaOptions, item.stamina),
        [&item, &staminaOptions, notify](int selected) {
            item.stamina = staminaOptions[static_cast<size_t>(selected)].value;
            notify();
        });
    staminaCell->addGestureRecognizer(new brls::TapGestureRecognizer(staminaCell));
    content->addView(staminaCell);

    const auto& pullOptions = HorseEnums::statPull();
    auto* pullCell = new brls::SelectorCell();
    pullCell->init(
        "Pull", HorseEnums::labelsFor(pullOptions), HorseEnums::indexForValue(pullOptions, item.pull),
        [&item, &pullOptions, notify](int selected) {
            item.pull = pullOptions[static_cast<size_t>(selected)].value;
            notify();
        });
    pullCell->addGestureRecognizer(new brls::TapGestureRecognizer(pullCell));
    content->addView(pullCell);

    addSectionHeader(content, "Breed");
    const auto& typeOptions = HorseEnums::horseTypes();
    auto* typeCell = new brls::SelectorCell();
    typeCell->init(
        "Horse Type", HorseEnums::labelsFor(typeOptions),
        HorseEnums::indexForValue(typeOptions, static_cast<int32_t>(item.horseType)),
        [&item, &typeOptions, notify](int selected) {
            item.horseType = static_cast<uint32_t>(typeOptions[static_cast<size_t>(selected)].value);
            notify();
        });
    typeCell->addGestureRecognizer(new brls::TapGestureRecognizer(typeCell));
    content->addView(typeCell);

    addBoundedField(
        content, "Color Type", static_cast<long>(item.colorType), 0, totk::GameLimits::kHorseColorTypeMax,
        [&item](long value) {
            item.colorType = static_cast<uint32_t>(std::max(0L, value));
            totk::GameLimits::clampHorseItem(item);
            return static_cast<long>(item.colorType);
        },
        [&item, notify](long value) {
            item.colorType = static_cast<uint32_t>(value);
            totk::GameLimits::clampHorseItem(item);
            notify();
        },
        "Palette index for normal horses");

    auto* footCell = new brls::SelectorCell();
    footCell->init(
        "Foot Type", {"Normal", "Heavy"}, static_cast<int>(item.footType > 0 ? 1 : 0),
        [&item, notify](int selected) {
            item.footType = static_cast<uint32_t>(selected);
            notify();
        });
    footCell->addGestureRecognizer(new brls::TapGestureRecognizer(footCell));
    content->addView(footCell);

    addSectionHeader(content, "Tack");
    const auto& maneOptions = HorseEnums::manes();
    auto* maneCell = new brls::SelectorCell();
    maneCell->init(
        "Mane", ItemEnums::labelsFor(maneOptions), ItemEnums::indexForValue(maneOptions, item.mane),
        [&item, &maneOptions, notify](int selected) {
            item.mane = maneOptions[static_cast<size_t>(selected)].value;
            notify();
        });
    maneCell->addGestureRecognizer(new brls::TapGestureRecognizer(maneCell));
    content->addView(maneCell);

    const auto& saddleOptions = HorseEnums::saddles();
    auto* saddleCell = new brls::SelectorCell();
    saddleCell->init(
        "Saddle", ItemEnums::labelsFor(saddleOptions), ItemEnums::indexForValue(saddleOptions, item.saddle),
        [&item, &saddleOptions, notify](int selected) {
            item.saddle = saddleOptions[static_cast<size_t>(selected)].value;
            notify();
        });
    saddleCell->addGestureRecognizer(new brls::TapGestureRecognizer(saddleCell));
    content->addView(saddleCell);

    const auto& reinOptions = HorseEnums::reins();
    auto* reinCell = new brls::SelectorCell();
    reinCell->init(
        "Reins", ItemEnums::labelsFor(reinOptions), ItemEnums::indexForValue(reinOptions, item.reins),
        [&item, &reinOptions, notify](int selected) {
            item.reins = reinOptions[static_cast<size_t>(selected)].value;
            notify();
        });
    reinCell->addGestureRecognizer(new brls::TapGestureRecognizer(reinCell));
    content->addView(reinCell);

    addSectionHeader(content, "Stable Portrait");
    const auto& patternOptions = HorseEnums::patterns();
    auto* patternCell = new brls::SelectorCell();
    patternCell->init(
        "Coat Pattern", ItemEnums::labelsFor(patternOptions), ItemEnums::indexForValue(patternOptions, item.iconPattern),
        [&item, &patternOptions, notify](int selected) {
            item.iconPattern = patternOptions[static_cast<size_t>(selected)].value;
            notify();
        });
    patternCell->addGestureRecognizer(new brls::TapGestureRecognizer(patternCell));
    content->addView(patternCell);

    const auto& eyeOptions = HorseEnums::eyeColors();
    auto* eyeCell = new brls::SelectorCell();
    eyeCell->init(
        "Eye Color", ItemEnums::labelsFor(eyeOptions), ItemEnums::indexForValue(eyeOptions, item.iconEyeColor),
        [&item, &eyeOptions, notify](int selected) {
            item.iconEyeColor = eyeOptions[static_cast<size_t>(selected)].value;
            notify();
        });
    eyeCell->addGestureRecognizer(new brls::TapGestureRecognizer(eyeCell));
    content->addView(eyeCell);

    const auto addColorField = [&](const std::string& title, uint32_t& channel) {
        addBoundedField(
            content, title, static_cast<long>(channel), 0, totk::GameLimits::kHorseIconColorMax,
            [&channel](long value) {
                channel = static_cast<uint32_t>(std::clamp(value, 0L, static_cast<long>(totk::GameLimits::kHorseIconColorMax)));
                return static_cast<long>(channel);
            },
            [&channel, &item, notify](long value) {
                channel = static_cast<uint32_t>(value);
                totk::GameLimits::clampHorseItem(item);
                notify();
            });
    };

    addSectionHeader(content, "Body Colors");
    addColorField("Primary Red", item.iconPrimaryColorRed);
    addColorField("Primary Green", item.iconPrimaryColorGreen);
    addColorField("Primary Blue", item.iconPrimaryColorBlue);
    addColorField("Secondary Red", item.iconSecondaryColorRed);
    addColorField("Secondary Green", item.iconSecondaryColorGreen);
    addColorField("Secondary Blue", item.iconSecondaryColorBlue);
    addColorField("Nose Red", item.iconNoseColorRed);
    addColorField("Nose Green", item.iconNoseColorGreen);
    addColorField("Nose Blue", item.iconNoseColorBlue);

    addSectionHeader(content, "Mane Colors");
    addColorField("Hair Primary Red", item.iconHairPrimaryColorRed);
    addColorField("Hair Primary Green", item.iconHairPrimaryColorGreen);
    addColorField("Hair Primary Blue", item.iconHairPrimaryColorBlue);
    addColorField("Hair Secondary Red", item.iconHairSecondaryColorRed);
    addColorField("Hair Secondary Green", item.iconHairSecondaryColorGreen);
    addColorField("Hair Secondary Blue", item.iconHairSecondaryColorBlue);
}

}  // namespace totk::ui
