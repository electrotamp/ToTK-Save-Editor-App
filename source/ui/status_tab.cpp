#include "ui/status_tab.hpp"

#include "app/app_state.hpp"
#include "save/game_limits.hpp"
#include "ui/app_theme.hpp"
#include "ui/bounded_numeric_cell.hpp"
#include "ui/editor_layout.hpp"
#include "ui/editor_theme.hpp"
#include "ui/focus_helpers.hpp"
#include "ui/theme_import_activity.hpp"

#include <string>
#include <vector>

namespace totk::ui {

namespace {

struct StaminaOption {
    uint32_t value;
    const char* label;
};

constexpr StaminaOption kStaminaOptions[] = {
    {1148846080u, "1 wheel"},
    {1150681088u, "1 wheel + 1/5"},
    {1152319488u, "1 wheel + 2/5"},
    {1153957888u, "1 wheel + 3/5"},
    {1155596288u, "1 wheel + 4/5"},
    {1157234688u, "2 wheels"},
    {1158250496u, "2 wheels + 1/5"},
    {1159069696u, "2 wheels + 2/5"},
    {1159888896u, "2 wheels + 3/5"},
    {1160708096u, "2 wheels + 4/5"},
    {1161527296u, "3 wheels"},
    {1342177279u, "Infinite (editor)"},
};

struct HeartOption {
    uint32_t quarters;
    std::string label;
};

struct BatteryOption {
    uint32_t value;
    std::string label;
};

totk::PlayerStats& editorStats() { return AppState::instance().editor().stats(); }

float listTextInset() { return brls::Application::getStyle()["brls/listitem/descriptionIndent"]; }

const std::vector<HeartOption>& heartOptions() {
    static const std::vector<HeartOption> options = [] {
        std::vector<HeartOption> result;
        for (uint32_t quarters = totk::GameLimits::kMaxLifeQuartersMin;
             quarters <= totk::GameLimits::kMaxLifeQuartersMax; quarters += 4) {
            const uint32_t hearts = quarters / 4;
            result.push_back({quarters, std::to_string(hearts) + (hearts == 1 ? " heart" : " hearts")});
        }
        return result;
    }();
    return options;
}

const std::vector<BatteryOption>& batteryOptions() {
    static const std::vector<BatteryOption> options = [] {
        std::vector<BatteryOption> result;
        for (uint32_t value = totk::GameLimits::kBatteryMin; value <= totk::GameLimits::kBatteryMax; value += 1000) {
            const uint32_t cells = value / 1000;
            result.push_back({value, std::to_string(cells) + (cells == 1 ? " energy cell" : " energy cells")});
        }
        return result;
    }();
    return options;
}

std::vector<std::string> heartLabels() {
    std::vector<std::string> labels;
    labels.reserve(heartOptions().size());
    for (const auto& option : heartOptions()) {
        labels.push_back(option.label);
    }
    return labels;
}

std::vector<std::string> staminaLabels() {
    std::vector<std::string> labels;
    labels.reserve(std::size(kStaminaOptions));
    for (const auto& option : kStaminaOptions) {
        labels.emplace_back(option.label);
    }
    return labels;
}

std::vector<std::string> batteryLabels() {
    std::vector<std::string> labels;
    labels.reserve(batteryOptions().size());
    for (const auto& option : batteryOptions()) {
        labels.push_back(option.label);
    }
    return labels;
}

void alignAsListText(brls::Label* label) {
    label->setMarginLeft(listTextInset());
    label->setMarginRight(listTextInset());
}

brls::Label* makeMetaLabel() {
    auto* label = new brls::Label();
    label->setFontSize(15);
    label->setTextColor(AppTheme::instance().colors().mutedText);
    label->setFocusable(false);
    alignAsListText(label);
    return label;
}

void addCell(brls::Box* parent, brls::View* cell) {
    cell->setWidthPercentage(100);
    parent->addView(cell);
}

void addSelector(brls::Box* parent, brls::SelectorCell* cell) {
    cell->setWidthPercentage(100);
    parent->addView(cell);
}

brls::View* findFirstFocusable(brls::View* root) {
    if (!root) return nullptr;

    if (auto* box = dynamic_cast<brls::Box*>(root)) {
        for (brls::View* child : box->getChildren()) {
            if (brls::View* focusable = findFirstFocusable(child)) {
                return focusable;
            }
        }
    }

    if (root->isFocusable()) return root;
    return nullptr;
}

int closestHeartSelection(uint32_t quarters) {
    const uint32_t clamped = totk::GameLimits::clampMaxLifeQuarters(static_cast<long>(quarters));
    const auto& options = heartOptions();
    int bestIndex = 0;
    uint32_t bestDistance = UINT32_MAX;
    for (size_t i = 0; i < options.size(); ++i) {
        const uint32_t distance = clamped >= options[i].quarters ? clamped - options[i].quarters
                                                                 : options[i].quarters - clamped;
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = static_cast<int>(i);
        }
    }
    return bestIndex;
}

}  // namespace

StatusTab::StatusTab() {
    this->setWidthPercentage(100);
    this->setHeightPercentage(100);
    this->setHideHighlightBackground(true);
    this->setHideHighlightBorder(true);
    this->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);

    content = new brls::Box();
    content->setAxis(brls::Axis::COLUMN);
    content->setWidthPercentage(100);
    content->setPadding(kEditorPagePaddingTop, kEditorPageInset, kEditorPagePaddingBottom, kEditorPageInset);

    versionLabel = makeMetaLabel();
    versionLabel->setText("Game version: -");
    versionLabel->setMarginBottom(4);
    content->addView(versionLabel);

    playtimeLabel = makeMetaLabel();
    playtimeLabel->setText("Playtime: -");
    playtimeLabel->setMarginBottom(12);
    content->addView(playtimeLabel);

    rupeesCell = new BoundedNumericCell();
    rupeesCell->init(
        "Rupees", 0, 0, totk::GameLimits::kRupeesMax,
        [](long value) { return static_cast<long>(totk::GameLimits::clampRupees(value)); },
        [](long value) { editorStats().rupees = static_cast<uint32_t>(value); }, "0 - 999999");
    addCell(content, rupeesCell);
    defaultFocusCell_ = rupeesCell;

    heartsCell = new brls::SelectorCell();
    heartsCell->init(
        "Hearts", heartLabels(), 0,
        [](int selected) { editorStats().maxHearts = heartOptions()[static_cast<size_t>(selected)].quarters; });
    addSelector(content, heartsCell);

    staminaCell = new brls::SelectorCell();
    staminaCell->init(
        "Stamina", staminaLabels(), 0,
        [](int selected) { editorStats().maxStamina = kStaminaOptions[selected].value; });
    addSelector(content, staminaCell);

    batteryCell = new brls::SelectorCell();
    batteryCell->init(
        "Battery", batteryLabels(), 0,
        [](int selected) {
            editorStats().maxBattery = static_cast<float>(batteryOptions()[static_cast<size_t>(selected)].value);
        });
    addSelector(content, batteryCell);

    ponyCell = new BoundedNumericCell();
    ponyCell->init(
        "Pony Points", 0, 0, totk::GameLimits::kPonyPointsMax,
        [](long value) { return static_cast<long>(totk::GameLimits::clampPonyPoints(value)); },
        [](long value) { editorStats().ponyPoints = static_cast<uint32_t>(value); }, "0 - 999999");
    addCell(content, ponyCell);

    pouchWeaponsCell = new BoundedNumericCell();
    pouchWeaponsCell->init(
        "Weapon Pouch Size", 9, totk::GameLimits::kPouchWeaponsMin, totk::GameLimits::kPouchWeaponsMax,
        [](long value) { return static_cast<long>(totk::GameLimits::clampPouchWeapons(value)); },
        [](long value) { editorStats().pouchWeapons = static_cast<uint32_t>(value); }, "9 - 20");
    addCell(content, pouchWeaponsCell);

    pouchBowsCell = new BoundedNumericCell();
    pouchBowsCell->init(
        "Bow Pouch Size", 5, totk::GameLimits::kPouchBowsMin, totk::GameLimits::kPouchBowsMax,
        [](long value) { return static_cast<long>(totk::GameLimits::clampPouchBows(value)); },
        [](long value) { editorStats().pouchBows = static_cast<uint32_t>(value); }, "5 - 14");
    addCell(content, pouchBowsCell);

    pouchShieldsCell = new BoundedNumericCell();
    pouchShieldsCell->init(
        "Shield Pouch Size", 4, totk::GameLimits::kPouchShieldsMin, totk::GameLimits::kPouchShieldsMax,
        [](long value) { return static_cast<long>(totk::GameLimits::clampPouchShields(value)); },
        [](long value) { editorStats().pouchShields = static_cast<uint32_t>(value); }, "4 - 20");
    addCell(content, pouchShieldsCell);

    constexpr long kCoordMin = -25000;
    constexpr long kCoordMax = 25000;

    posXCell = new BoundedNumericCell();
    posXCell->init(
        "Position X", 0, kCoordMin, kCoordMax,
        [](long value) { return value; },
        [](long value) { editorStats().savePos.x = static_cast<float>(value); }, "", true);
    addCell(content, posXCell);

    posYCell = new BoundedNumericCell();
    posYCell->init(
        "Position Y", 0, kCoordMin, kCoordMax,
        [](long value) { return value; },
        [](long value) { editorStats().savePos.y = static_cast<float>(value); }, "", true);
    addCell(content, posYCell);

    posZCell = new BoundedNumericCell();
    posZCell->init(
        "Position Z", 0, kCoordMin, kCoordMax,
        [](long value) { return value; },
        [](long value) { editorStats().savePos.z = static_cast<float>(value); }, "", true);
    addCell(content, posZCell);

    mapPinsLabel = makeMetaLabel();
    mapPinsLabel->setText("Map pins: 0 / 300");
    mapPinsLabel->setMarginTop(8);
    mapPinsLabel->setMarginBottom(8);
    content->addView(mapPinsLabel);

    themeLabel = makeMetaLabel();
    themeLabel->setText("Editor theme: Default");
    themeLabel->setMarginBottom(4);
    content->addView(themeLabel);

    auto* importTheme = new brls::Button();
    importTheme->setText("Import Editor Theme");
    importTheme->setMarginLeft(listTextInset());
    importTheme->setMarginBottom(8);
    importTheme->registerClickAction([this](brls::View*) {
        pushThemeImportPicker([this](bool success, const std::string& message) {
            if (success) {
                applyThemeColors();
            }
            brls::Application::notify(message);
        });
        return true;
    });
    content->addView(importTheme);

    auto* removePins = new brls::Button();
    removePins->setText("Remove All Map Pins");
    removePins->setMarginLeft(listTextInset());
    removePins->registerClickAction([](brls::View*) {
        AppState::instance().editor().removeAllMapPins();
        brls::Application::notify("All map pins removed");
        return true;
    });
    content->addView(removePins);

    this->setContentView(content);
    refresh();
}

void StatusTab::activatePageFocus() {
    focus::focusView(defaultFocusCell_ ? defaultFocusCell_ : findFirstFocusable(content));
}

void StatusTab::onChildFocusGained(brls::View* directChild, brls::View* focusedView) {
    brls::Box::onChildFocusGained(directChild, focusedView);

    childFocused = true;

    if (brls::Application::getInputType() == brls::InputType::GAMEPAD
        && behavior == brls::ScrollingBehavior::CENTERED) {
        updateScrolling(false);
    }
}

brls::View* StatusTab::getNextFocus(brls::FocusDirection direction, brls::View* currentView) {
    const float bottomLimit = getContentHeight() - getScrollingAreaHeight();
    const float offsetY = getContentOffsetY();

    if (direction == brls::FocusDirection::DOWN) {
        if (offsetY < bottomLimit - 0.01f) {
            return this;
        }
        return brls::Box::getNextFocus(direction, currentView);
    }

    if (direction == brls::FocusDirection::UP) {
        if (offsetY > 0.01f) {
            return this;
        }
        return brls::Box::getNextFocus(direction, currentView);
    }

    return brls::ScrollingFrame::getNextFocus(direction, currentView);
}

brls::View* StatusTab::getDefaultFocus() {
    // CENTERED scroll uses getDefaultFocus() to pick the row to keep on screen.
    // Follow the active row during navigation; only fall back to Rupees at boot.
    if (brls::View* current = brls::Application::getCurrentFocus()) {
        if (content && focus::isDescendantOf(current, content)) {
            if (brls::View* target = current->getDefaultFocus()) {
                return target;
            }
            return current;
        }
    }

    if (defaultFocusCell_ && defaultFocusCell_->isFocusable()) {
        return defaultFocusCell_;
    }

    if (content) {
        if (brls::View* focusable = findFirstFocusable(content)) {
            return focusable;
        }
        if (brls::View* focus = content->getDefaultFocus()) {
            return focus;
        }
    }
    return brls::ScrollingFrame::getDefaultFocus();
}

void StatusTab::willAppear(bool resetState) {
    brls::ScrollingFrame::willAppear(resetState);
}

int StatusTab::heartSelectionForValue(uint32_t quarters) const {
    return closestHeartSelection(quarters);
}

int StatusTab::staminaSelectionForValue(uint32_t value) const {
    const uint32_t clamped = totk::GameLimits::clampMaxStamina(static_cast<long>(value));
    for (size_t i = 0; i < std::size(kStaminaOptions); ++i) {
        if (kStaminaOptions[i].value == clamped) return static_cast<int>(i);
    }
    return static_cast<int>(std::size(kStaminaOptions)) - 2;
}

int StatusTab::batterySelectionForValue(float value) const {
    const float clamped = totk::GameLimits::clampBattery(static_cast<long>(value));
    const auto& options = batteryOptions();
    int bestIndex = 0;
    float bestDistance = 1.0e9f;
    for (size_t i = 0; i < options.size(); ++i) {
        const float distance = std::abs(clamped - static_cast<float>(options[i].value));
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = static_cast<int>(i);
        }
    }
    return bestIndex;
}

void StatusTab::refresh() {
    if (themeLabel) {
        themeLabel->setText("Editor theme: " + EditorTheme::instance().name());
    }

    auto& editor = AppState::instance().editor();
    if (!editor.isLoaded()) return;

    totk::GameLimits::clampPlayerStats(editor.stats());
    const auto& stats = editor.stats();

    versionLabel->setText("Game version: " + editor.gameVersion());
    playtimeLabel->setText("Playtime: " + stats.playtime);
    rupeesCell->setValue(stats.rupees, true);
    heartsCell->setSelection(heartSelectionForValue(stats.maxHearts), true);
    staminaCell->setSelection(staminaSelectionForValue(stats.maxStamina), true);
    batteryCell->setSelection(batterySelectionForValue(stats.maxBattery), true);
    ponyCell->setValue(stats.ponyPoints, true);
    pouchWeaponsCell->setValue(stats.pouchWeapons, true);
    pouchBowsCell->setValue(stats.pouchBows, true);
    pouchShieldsCell->setValue(stats.pouchShields, true);
    posXCell->setValue(static_cast<long>(stats.savePos.x), true);
    posYCell->setValue(static_cast<long>(stats.savePos.y), true);
    posZCell->setValue(static_cast<long>(stats.savePos.z), true);
    mapPinsLabel->setText("Map pins: " + std::to_string(stats.mapPinCount) + " / 300");
}

void StatusTab::applyThemeColors() {
    const auto& colors = AppTheme::instance().colors();
    if (versionLabel) versionLabel->setTextColor(colors.mutedText);
    if (playtimeLabel) playtimeLabel->setTextColor(colors.mutedText);
    if (mapPinsLabel) mapPinsLabel->setTextColor(colors.mutedText);
    if (themeLabel) {
        themeLabel->setTextColor(colors.mutedText);
        themeLabel->setText("Editor theme: " + EditorTheme::instance().name());
    }
}

void StatusTab::apply() {
    auto& stats = editorStats();
    stats.rupees = totk::GameLimits::clampRupees(rupeesCell->getValue());
    stats.maxHearts = heartOptions()[static_cast<size_t>(heartsCell->getSelection())].quarters;
    stats.maxStamina = kStaminaOptions[staminaCell->getSelection()].value;
    stats.maxBattery = static_cast<float>(batteryOptions()[static_cast<size_t>(batteryCell->getSelection())].value);
    stats.ponyPoints = totk::GameLimits::clampPonyPoints(ponyCell->getValue());
    stats.pouchWeapons = totk::GameLimits::clampPouchWeapons(pouchWeaponsCell->getValue());
    stats.pouchBows = totk::GameLimits::clampPouchBows(pouchBowsCell->getValue());
    stats.pouchShields = totk::GameLimits::clampPouchShields(pouchShieldsCell->getValue());
    stats.savePos.x = static_cast<float>(posXCell->getValue());
    stats.savePos.y = static_cast<float>(posYCell->getValue());
    stats.savePos.z = static_cast<float>(posZCell->getValue());
}

brls::View* StatusTab::create() { return new StatusTab(); }

}  // namespace totk::ui
