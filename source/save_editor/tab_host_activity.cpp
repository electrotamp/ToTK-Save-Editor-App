#include "save_editor/tab_host_activity.hpp"

#include "app/app_state.hpp"
#include "save_editor/save_editor_item_picker.hpp"
#include "save_editor/save_editor_pouch.hpp"
#include "save_editor/save_editor_stats.hpp"
#include "save_editor/save_editor_weapon_edit.hpp"
#include "save_editor/tab_order.hpp"
#include "save/game_limits.hpp"
#include "save/save_editor.hpp"
#include "platform/switch_save_mount.hpp"
#include "ui/autobuild_card.hpp"
#include "ui/autobuild_catalog_activity.hpp"
#include "ui/autobuild_icon_store.hpp"
#include "ui/autobuild_import_activity.hpp"
#include "ui/bounded_numeric_cell.hpp"
#include "ui/item_database.hpp"
#include "ui/pouch_item_display.hpp"
#include "ui/zonai_wisp_overlay.hpp"
#include "util/debug_stage.hpp"
#include "util/icon_atlas.hpp"
#include "util/image_loader.hpp"
#include "util/perf_trace.hpp"
#include "util/totk_log.hpp"

#include <borealis/core/application.hpp>
#include <borealis/core/touch/tap_gesture.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

namespace totk::save_editor {

namespace {

// Same locked-zoom tile geometry as ItemPickerActivity — see the old
// EquipmentActivity/PouchActivity's matching comment (git history).
constexpr int kGridColumns = 9;
constexpr float kTileIconPx = 114.0f;
constexpr float kTileWidth = 123.0f;
constexpr float kTileHeight = 142.0f;
constexpr float kTileMargin = 4.8f;
constexpr float kTileCornerRadius = 7.2f;
constexpr float kBadgeFontSize = 17.0f;
constexpr int kAutobuildGridColumns = 3;

bool isEquipmentCategory(const std::string& category) {
    return category == "weapons" || category == "shields" || category == "bows";
}

void wireGridFocus(const std::vector<brls::View*>& tiles, int columns) {
    if (tiles.empty() || columns <= 0) return;
    const int count = static_cast<int>(tiles.size());
    const int rows = (count + columns - 1) / columns;

    auto at = [&](int row, int col) -> brls::View* {
        const int index = row * columns + col;
        if (index < 0 || index >= count) return nullptr;
        return tiles[static_cast<size_t>(index)];
    };

    for (int row = 0; row < rows; ++row) {
        const int colsInRow = std::min(columns, count - row * columns);
        for (int col = 0; col < colsInRow; ++col) {
            brls::View* tile = at(row, col);
            if (!tile) continue;

            if (row == 0) {
                tile->setCustomNavigationRoute(brls::FocusDirection::UP, tile);
            } else if (brls::View* up = at(row - 1, col)) {
                tile->setCustomNavigationRoute(brls::FocusDirection::UP, up);
            }
            if (row + 1 >= rows) {
                tile->setCustomNavigationRoute(brls::FocusDirection::DOWN, tile);
            } else if (brls::View* down = at(row + 1, col)) {
                tile->setCustomNavigationRoute(brls::FocusDirection::DOWN, down);
            }
            if (col == 0) {
                tile->setCustomNavigationRoute(brls::FocusDirection::LEFT, tile);
            } else if (brls::View* left = at(row, col - 1)) {
                tile->setCustomNavigationRoute(brls::FocusDirection::LEFT, left);
            }
            if (col + 1 >= colsInRow) {
                tile->setCustomNavigationRoute(brls::FocusDirection::RIGHT, tile);
            } else if (brls::View* right = at(row, col + 1)) {
                tile->setCustomNavigationRoute(brls::FocusDirection::RIGHT, right);
            }
        }
    }
}

std::string categoryDisplaySingular(const std::string& category) {
    if (category == "shields") return "Shield";
    if (category == "bows") return "Bow";
    return "Weapon";
}

std::string categoryDisplaySingularLower(const std::string& category) {
    if (category == "shields") return "shield";
    if (category == "bows") return "bow";
    return "weapon";
}

std::string pouchDisplaySingular(const std::string& category) {
    if (category == "armors") return "Armor";
    if (category == "materials") return "Material";
    if (category == "food") return "Food";
    if (category == "devices") return "Device";
    if (category == "key") return "Key Item";
    if (category == "horses") return "Horse";
    return category;
}

// Plural noun used for the "No X" empty-state message — differs from
// tabDisplayName() for "key" ("Key Items" the tab title vs "key items" here).
std::string pouchEmptyPlural(const std::string& category) {
    if (category == "key") return "key items";
    return category;
}

struct HeartOption {
    uint32_t quarters;
    std::string label;
};

struct BatteryOption {
    uint32_t value;
    std::string label;
};

totk::PlayerStats& editorStats() { return AppState::instance().editor().stats(); }

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
    for (const auto& option : heartOptions()) labels.push_back(option.label);
    return labels;
}

std::vector<std::string> batteryLabels() {
    std::vector<std::string> labels;
    for (const auto& option : batteryOptions()) labels.push_back(option.label);
    return labels;
}

int closestHeartSelection(uint32_t quarters) {
    const uint32_t clamped = totk::GameLimits::clampMaxLifeQuarters(static_cast<long>(quarters));
    const auto& options = heartOptions();
    int bestIndex = 0;
    uint32_t bestDistance = UINT32_MAX;
    for (size_t i = 0; i < options.size(); ++i) {
        const uint32_t distance =
            clamped >= options[i].quarters ? clamped - options[i].quarters : options[i].quarters - clamped;
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = static_cast<int>(i);
        }
    }
    return bestIndex;
}

int batterySelectionForValue(float value) {
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

brls::Label* makeMetaLabel() {
    auto* label = new brls::Label();
    label->setFontSize(15);
    label->setTextColor(nvgRGB(160, 160, 160));
    label->setFocusable(false);
    return label;
}

void addCell(brls::Box* parent, brls::View* cell) {
    cell->setWidthPercentage(100);
    parent->addView(cell);
}

}  // namespace

TabHostActivity::TabHostActivity(std::string initialTabId) : initialTabId_(std::move(initialTabId)) {}

brls::View* TabHostActivity::createContentView() {
    TOTK_PERF_SCOPE("tabhost.createContentView");
    root_ = new brls::Box();
    root_->setAxis(brls::Axis::COLUMN);
    root_->setWidthPercentage(100);
    root_->setHeightPercentage(100);
    root_->setGrow(1.0f);
    root_->setFocusable(false);
    root_->setPadding(12.0f, 40.0f, 28.0f, 40.0f);

    contentRoot_ = new brls::Box();
    contentRoot_->setAxis(brls::Axis::COLUMN);
    contentRoot_->setWidthPercentage(100);
    contentRoot_->setGrow(1.0f);
    contentRoot_->setFocusable(false);
    root_->addView(contentRoot_);

    auto* frame = new brls::AppletFrame(root_);
    // Attached to the AppletFrame itself, not root_ — root_ is only
    // AppletFrame's "content" slot, confined between the header and footer
    // bars, so a wallpaper/wisp sized to root_ stopped short of the true
    // screen edges. Neither the header Box nor the footer BottomBar paints
    // its own opaque background (see their draw() overrides — just a border
    // line and text), so attaching here, behind everything at index 0, lets
    // the wallpaper and wisps show through the full window including behind
    // both bars. Attached exactly once — unlike the old per-tab Activities,
    // this survives every tab switch untouched; only contentRoot_ above gets
    // cleared and rebuilt per switch.
    totk::ui::attachEditorBackground(frame);

    // See the header comment on tabTitleLabel_. 28px/header_height=88 match
    // the frame's own title style (style.cpp) so this reads identically to
    // the label it replaces, just centered instead of left-aligned.
    tabTitleLabel_ = new brls::Label();
    tabTitleLabel_->setFontSize(28.0f);
    tabTitleLabel_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    tabTitleLabel_->setFocusable(false);
    tabTitleLabel_->setPositionType(brls::PositionType::ABSOLUTE);
    tabTitleLabel_->setPositionTop(0);
    tabTitleLabel_->setPositionLeft(0);
    tabTitleLabel_->setPositionRight(0);
    tabTitleLabel_->setPositionBottom(0);
    frame->getHeader()->addView(tabTitleLabel_);

    return frame;
}

void TabHostActivity::onContentAvailable() {
    // Build synchronously, not deferred: pushActivity() calls
    // giveFocus(getDefaultFocus()) right after this returns, so the first
    // tile/cell needs to already exist (see the old EquipmentActivity's
    // matching comment, git history).
    switchToTab(initialTabId_);
}

void TabHostActivity::onResume() {
    // Fires when ItemPickerActivity/EquipmentEditActivity/PouchEditActivity
    // pop back to this activity — rebuild the current tab in place to pick
    // up whatever changed (added/removed/edited item, or edited stats).
    switchToTab(currentTabId_);
}

void TabHostActivity::willDisappear(bool resetState) {
    // Unsubscribe before any teardown starts, not in the destructor: destroying
    // a focused tile can trigger a focus reassignment that re-fires this same
    // global event mid-teardown, invoking a still-subscribed callback on a
    // half-destroyed object.
    if (focusSubscribed_) {
        brls::Application::getGlobalFocusChangeEvent()->unsubscribe(focusSubscription_);
        focusSubscribed_ = false;
    }
    brls::Activity::willDisappear(resetState);
}

TabHostActivity::~TabHostActivity() {
    TOTK_STAGE("editor.tabhost.destroy.begin tab=%s tiles=%zu", currentTabId_.c_str(), tiles_.size());
    if (focusSubscribed_) {
        brls::Application::getGlobalFocusChangeEvent()->unsubscribe(focusSubscription_);
        focusSubscribed_ = false;
    }
}

void openTabHost(const std::string& initialTabId) {
    TOTK_LOG("editor: open tab host initial=%s", initialTabId.c_str());
    brls::Application::pushActivity(new TabHostActivity(initialTabId), brls::TransitionAnimation::NONE);
}

void TabHostActivity::switchToTab(const std::string& tabId) {
    TOTK_PERF_SCOPE("tabhost.switchToTab.total");
    TOTK_STAGE("editor.tabhost.switch tab=%s", tabId.c_str());

    // See the header comment on buildGeneration_ — invalidates any
    // still-pending deferred focus-set callback from a prior, now-superseded
    // switch before we touch anything.
    ++buildGeneration_;

    if (currentTabId_ == "stats" && tabId != currentTabId_) {
        applyStatsToSave();
    }

    currentTabId_ = tabId;
    wireFrameChrome(tabId);

    if (tabId == "stats") {
        buildStatsForm();
    } else if (tabId == "autobuild") {
        buildAutobuildTab();
    } else if (tabId == "credits") {
        buildCreditsTab();
    } else if (isEquipmentCategory(tabId)) {
        buildEquipmentGrid(tabId);
    } else {
        buildPouchGrid(tabId);
    }

    TOTK_LOG("editor: tab switched to %s", tabId.c_str());
}

void TabHostActivity::setAddAction(bool enabled, const std::string& label, brls::ActionListener listener) {
    auto* frame = dynamic_cast<brls::AppletFrame*>(this->getContentView());
    if (!frame) return;

    if (addActionRegistered_) {
        frame->unregisterAction(addActionId_);
        addActionRegistered_ = false;
    }
    if (enabled) {
        addActionId_ = frame->registerAction(label, brls::BUTTON_X, listener);
        addActionRegistered_ = true;
    }
}

void TabHostActivity::setNeighborActions(const std::string& tabId) {
    auto* frame = dynamic_cast<brls::AppletFrame*>(this->getContentView());
    if (!frame) return;

    if (prevActionRegistered_) {
        frame->unregisterAction(prevActionId_);
        prevActionRegistered_ = false;
    }
    if (nextActionRegistered_) {
        frame->unregisterAction(nextActionId_);
        nextActionRegistered_ = false;
    }

    std::string prevId, nextId;
    tabNeighbors(tabId, prevId, nextId);

    // Deferred, not called synchronously: Borealis's Application::handleAction
    // runs a live `for (auto& action : hintParent->getActions())` over this
    // same frame's actions vector while invoking whichever listener matched
    // the pressed button (application.cpp). switchToTab() ends up back in
    // wireFrameChrome()/setNeighborActions(), which calls
    // frame->unregisterAction() on this very vector — erasing an element out
    // from under an iterator that's still walking it is undefined behavior,
    // and reliably crashed under fast repeated L/R presses. The old per-tab
    // Activities never hit this: each switch destroyed the whole frame (a
    // fresh object, fresh vector) rather than mutating the one currently
    // being iterated. Routing through brls::sync moves the mutation to the
    // next frame, cleanly outside handleAction's call stack.
    if (!prevId.empty()) {
        prevActionId_ = frame->registerAction(tabDisplayName(prevId), brls::BUTTON_LB, [this, prevId](brls::View*) {
            brls::sync([this, prevId]() { switchToTab(prevId); });
            return true;
        });
        prevActionRegistered_ = true;
    }
    if (!nextId.empty()) {
        nextActionId_ = frame->registerAction(tabDisplayName(nextId), brls::BUTTON_RB, [this, nextId](brls::View*) {
            brls::sync([this, nextId]() { switchToTab(nextId); });
            return true;
        });
        nextActionRegistered_ = true;
    }
}

void TabHostActivity::wireFrameChrome(const std::string& tabId) {
    auto* frame = dynamic_cast<brls::AppletFrame*>(this->getContentView());
    if (!frame) return;

    // Frame's own title left empty — see tabTitleLabel_'s header comment;
    // that centered label is what actually shows the tab name.
    frame->setTitle("");
    if (tabTitleLabel_) tabTitleLabel_->setText(tabDisplayName(tabId));

    frame->registerAction("Back", brls::BUTTON_B, [](brls::View*) {
        TOTK_STAGE("editor.tabhost.back.pressed");
        brls::Application::popActivity(brls::TransitionAnimation::NONE);
        TOTK_STAGE("editor.tabhost.back.popActivity.returned");
        brls::sync([]() { TOTK_STAGE("editor.picker.post_pop.next_frame_reached"); });
        return true;
    });

    frame->registerAction("Save", brls::BUTTON_Y, [](brls::View*) {
        TOTK_LOG("editor: save requested (Y)");
        auto& state = AppState::instance();
        if (state.editor().saveProgress(state.currentSavePath())) {
            brls::Application::notify("Save written successfully");
            TOTK_LOG("editor: save ok path=%s", state.currentSavePath().c_str());
        } else {
            brls::Application::notify("Failed to save");
            TOTK_LOG("editor: save failed path=%s", state.currentSavePath().c_str());
        }
        return true;
    });

    if (tabId == "stats") {
        setAddAction(false, "", nullptr);
    } else if (tabId == "credits") {
        setAddAction(false, "", nullptr);
    } else if (tabId == "autobuild") {
        setAddAction(true, "Import from SD", [](brls::View*) {
            totk::ui::pushAutobuildImportActivity();
            return true;
        });
    } else if (isEquipmentCategory(tabId)) {
        const std::string category = tabId;
        setAddAction(true, "Add " + categoryDisplaySingular(category), [this, category](brls::View*) {
            openEquipmentAddPicker(category);
            return true;
        });
    } else {
        const std::string category = tabId;
        setAddAction(true, "Add " + pouchDisplaySingular(category), [this, category](brls::View*) {
            openPouchAddPicker(category);
            return true;
        });
    }

    setNeighborActions(tabId);
}

// ---------------------------------------------------------------------------
// Equipment grid (weapons/shields/bows)

void TabHostActivity::buildEquipmentGrid(const std::string& category) {
    TOTK_PERF_SCOPE("tabhost.buildEquipmentGrid.total");
    // These belong to the outgoing tab's view tree. Bows intentionally do
    // not create a Fuse panel, so leaving either pointer intact would make
    // updateEquipmentHeader() dereference a destroyed weapon/shield view
    // immediately after clearViews().
    headerFuseIcon_ = nullptr;
    headerFuseLabel_ = nullptr;
    contentRoot_->clearViews();
    tiles_.clear();

    auto* root = contentRoot_;
    auto& editor = AppState::instance().editor();
    auto& db = ItemDatabase::instance();
    const auto it = editor.equipment().find(category);

    auto* topBar = new brls::Box();
    topBar->setAxis(brls::Axis::ROW);
    topBar->setWidthPercentage(100);
    topBar->setJustifyContent(brls::JustifyContent::FLEX_END);
    topBar->setAlignItems(brls::AlignItems::CENTER);
    topBar->setMarginBottom(4);
    topBar->setShrink(0);
    topBar->setFocusable(false);

    auto* countLabel = new brls::Label();
    countLabel->setFontSize(16);
    countLabel->setTextColor(nvgRGB(200, 200, 200));
    countLabel->setShrink(0);
    countLabel->setFocusable(false);
    const size_t equippedCount = it != editor.equipment().end() ? it->second.size() : 0;
    countLabel->setText(std::to_string(equippedCount) + " / " + std::to_string(editor.equipmentCapacity(category)));
    topBar->addView(countLabel);
    root->addView(topBar);

    auto* headerRow = new brls::Box();
    headerRow->setAxis(brls::Axis::ROW);
    headerRow->setAlignItems(brls::AlignItems::CENTER);
    headerRow->setWidthPercentage(100);
    headerRow->setMarginBottom(8);
    headerRow->setFocusable(false);

    headerIcon_ = new brls::Image();
    headerIcon_->setScalingType(brls::ImageScalingType::FIT);
    headerIcon_->setWidth(64);
    headerIcon_->setHeight(64);
    headerIcon_->setFocusable(false);
    headerIcon_->setMarginRight(12);
    headerRow->addView(headerIcon_);

    auto* headerText = new brls::Box();
    headerText->setAxis(brls::Axis::COLUMN);
    headerText->setGrow(1.0f);
    headerText->setFocusable(false);

    headerName_ = new brls::Label();
    headerName_->setFontSize(20);
    headerName_->setFocusable(false);
    headerText->addView(headerName_);

    headerSubtitle_ = new brls::Label();
    headerSubtitle_->setFontSize(15);
    headerSubtitle_->setTextColor(nvgRGB(160, 160, 160));
    headerSubtitle_->setFocusable(false);
    headerText->addView(headerSubtitle_);

    headerRow->addView(headerText);

    // Bows use arrow attachments, not the weapon/shield Fuse save field.
    if (category != "bows") {
        auto* fuseColumn = new brls::Box();
        fuseColumn->setAxis(brls::Axis::COLUMN);
        fuseColumn->setAlignItems(brls::AlignItems::CENTER);
        fuseColumn->setFocusable(false);
        fuseColumn->setMarginLeft(12);

        auto* fuseTitle = new brls::Label();
        fuseTitle->setText("Fuse");
        fuseTitle->setFontSize(12);
        fuseTitle->setTextColor(nvgRGB(160, 160, 160));
        fuseTitle->setFocusable(false);
        fuseTitle->setMarginBottom(4);
        fuseColumn->addView(fuseTitle);

        auto* fusePanel = new brls::Box();
        fusePanel->setAxis(brls::Axis::COLUMN);
        fusePanel->setAlignItems(brls::AlignItems::CENTER);
        fusePanel->setJustifyContent(brls::JustifyContent::CENTER);
        fusePanel->setFocusable(false);
        fusePanel->setBackgroundColor(nvgRGBA(255, 255, 255, 16));
        fusePanel->setCornerRadius(6.0f);
        fusePanel->setPadding(4.0f);

        headerFuseIcon_ = new brls::Image();
        headerFuseIcon_->setScalingType(brls::ImageScalingType::FIT);
        headerFuseIcon_->setWidth(48);
        headerFuseIcon_->setHeight(48);
        headerFuseIcon_->setFocusable(false);
        fusePanel->addView(headerFuseIcon_);

        headerFuseLabel_ = new brls::Label();
        headerFuseLabel_->setFontSize(14);
        headerFuseLabel_->setFocusable(false);
        fusePanel->addView(headerFuseLabel_);
        fuseColumn->addView(fusePanel);

        headerRow->addView(fuseColumn);
    }
    root->addView(headerRow);

    static const std::vector<totk::EquipmentItem> kNoItems;
    const auto& weapons = it != editor.equipment().end() ? it->second : kNoItems;
    const size_t capacity = editor.equipmentCapacity(category);
    const bool isFull = weapons.size() >= capacity;

    if (weapons.empty()) {
        headerName_->setText("No " + category);
        headerSubtitle_->setText("Select the + tile to add one");
        headerFuseLabel_->setText("-");
    }

    auto* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1.0f);
    scroll->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    scroll->setFocusable(false);
    scroll->setWidth(1224.0f);
    scroll->setAlignSelf(brls::AlignSelf::FLEX_START);

    auto* content = new brls::Box();
    content->setAxis(brls::Axis::COLUMN);
    content->setWidthPercentage(100);
    content->setFocusable(false);

    tiles_.reserve(weapons.size() + 1);
    const size_t totalSlots = weapons.size() + 1;
    for (size_t i = 0; i < totalSlots; ++i) {
        if (i % static_cast<size_t>(kGridColumns) == 0) {
            auto* rowBox = new brls::Box();
            rowBox->setAxis(brls::Axis::ROW);
            rowBox->setFocusable(false);
            content->addView(rowBox);
        }

        auto* rowBox = dynamic_cast<brls::Box*>(content->getChildren().back());
        if (!rowBox) continue;

        auto* tile = new brls::Box();
        tile->setAxis(brls::Axis::COLUMN);
        tile->setAlignItems(brls::AlignItems::CENTER);
        tile->setJustifyContent(brls::JustifyContent::CENTER);
        tile->setWidth(kTileWidth);
        tile->setHeight(kTileHeight);
        tile->setMargins(kTileMargin, kTileMargin, kTileMargin, kTileMargin);
        tile->setPadding(kTileMargin);
        tile->setCornerRadius(kTileCornerRadius);
        tile->setFocusable(true);
        tile->setBackgroundColor(nvgRGBA(255, 255, 255, 12));

        if (i == weapons.size()) {
            auto* plus = new brls::Label();
            plus->setFocusable(false);
            plus->setFontSize(isFull ? 16.0f : 28.0f);
            plus->setHorizontalAlign(brls::HorizontalAlign::CENTER);
            plus->setText(isFull ? "FULL" : "+");
            plus->setTextColor(isFull ? nvgRGB(220, 90, 90) : nvgRGB(230, 230, 230));
            tile->addView(plus);

            tile->registerAction("Add", brls::BUTTON_A, [this, category, isFull](brls::View*) {
                if (isFull) {
                    brls::Application::notify(categoryDisplaySingular(category) + " pouch is full");
                    return true;
                }
                openEquipmentAddPicker(category);
                return true;
            });
            tile->addGestureRecognizer(new brls::TapGestureRecognizer(tile));

            rowBox->addView(tile);
            tiles_.push_back(tile);
            continue;
        }

        const auto& item = weapons[i];

        auto* badge = new brls::Label();
        badge->setFocusable(false);
        badge->setFontSize(kBadgeFontSize);
        badge->setHorizontalAlign(brls::HorizontalAlign::LEFT);
        badge->setWidth(kTileIconPx);
        std::string badgeText = std::to_string(item.durability);
        badge->setText(badgeText);
        tile->addView(badge);

        auto* image = new brls::Image();
        image->setScalingType(brls::ImageScalingType::FIT);
        image->setWidth(kTileIconPx);
        image->setHeight(kTileIconPx);
        image->setFocusable(false);

        const auto paths = db.iconPathCandidates(category, item.id);
        bool iconOk = false;
        for (const auto& path : paths) {
            if (IconAtlas::instance().apply(image, path)) {
                iconOk = true;
                break;
            }
        }

        if (iconOk) {
            tile->addView(image);
        } else {
            TOTK_LOG("editor: missing icon for %s", item.id.c_str());
            delete image;
            const std::string itemName = db.nameForId(item.id);
            auto* nameLabel = new brls::Label();
            nameLabel->setFocusable(false);
            nameLabel->setFontSize(12);
            nameLabel->setText(itemName.empty() ? item.id : itemName);
            nameLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
            nameLabel->setWidth(kTileIconPx);
            nameLabel->setIsWrapping(true);
            tile->addView(nameLabel);
        }

        const size_t itemIndex = i;
        tile->registerAction("Select", brls::BUTTON_A, [this, category, itemIndex](brls::View*) {
            brls::Application::pushActivity(new EquipmentEditActivity(category, itemIndex),
                                             brls::TransitionAnimation::NONE);
            return true;
        });
        tile->addGestureRecognizer(new brls::TapGestureRecognizer(tile));

        rowBox->addView(tile);
        tiles_.push_back(tile);
    }

    scroll->setContentView(content);
    root->addView(scroll);

    wireGridFocus(tiles_, kGridColumns);
    updateEquipmentHeader(0);
    ensureGridFocusSubscription();

    TOTK_LOG("editor: grid built tiles=%zu", tiles_.size());

    // Immediate: unlike the old per-tab Activities (where pushActivity()'s
    // own giveFocus(getDefaultFocus()) covered the gap between this build and
    // the next real frame), switchToTab() is called directly with nothing
    // else claiming focus — deferring this by a frame left it dangling near
    // the AppletFrame's title for a frame ("ghost focus").
    if (!tiles_.empty()) {
        brls::Application::giveFocus(tiles_.front());
    }
    // Deferred + generation-guarded: kept as a same-frame-safe fixup for the
    // very first open (which — unlike later switches — DOES run inside
    // pushActivity()'s synchronous chain, so Borealis's own default-focus
    // call can still override the line above; this runs strictly after it).
    const uint32_t generation = buildGeneration_;
    brls::sync([this, generation]() {
        if (generation != buildGeneration_) return;
        if (!tiles_.empty()) {
            brls::Application::giveFocus(tiles_.front());
            TOTK_LOG("editor: focus on first tile");
        }
    });
}

void TabHostActivity::updateEquipmentHeader(size_t index) {
    auto& editor = AppState::instance().editor();
    const auto it = editor.equipment().find(currentTabId_);
    if (it == editor.equipment().end() || index >= it->second.size()) return;

    const auto info = totk::displayInfoForPouchItem(editor, currentTabId_, static_cast<int>(index));
    if (headerName_) headerName_->setText(info.name.empty() ? info.id : info.name);
    if (headerSubtitle_) headerSubtitle_->setText(info.subtitle);
    if (headerIcon_) {
        headerIcon_->clear();
        for (const auto& path : info.iconPaths) {
            if (IconAtlas::instance().apply(headerIcon_, path)) break;
        }
    }
    if (headerFuseIcon_ && !info.fuseId.empty()) {
        headerFuseIcon_->clear();
        bool iconOk = false;
        for (const auto& path : info.fuseIconPaths) {
            if (IconAtlas::instance().apply(headerFuseIcon_, path)) {
                iconOk = true;
                break;
            }
        }
        headerFuseIcon_->setVisibility(iconOk ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        if (headerFuseLabel_) {
            headerFuseLabel_->setText(iconOk ? "" : (info.fuseName.empty() ? info.fuseId : info.fuseName));
            headerFuseLabel_->setVisibility(iconOk ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
        }
    } else if (headerFuseLabel_) {
        if (headerFuseIcon_) {
            headerFuseIcon_->clear();
            headerFuseIcon_->setVisibility(brls::Visibility::GONE);
        }
        headerFuseLabel_->setText("None");
        headerFuseLabel_->setVisibility(brls::Visibility::VISIBLE);
    }
}

void TabHostActivity::openEquipmentAddPicker(const std::string& category) {
    brls::Application::pushActivity(
        save_editor::ItemPickerActivity::forCategory(
            category, "Add " + categoryDisplaySingular(category),
            [this, category](const std::string& id) {
                if (id.empty()) return;
                // Runs synchronously, before the picker pops (see
                // ItemPickerActivity::selectItem) — safe here since this is a
                // normal main-thread input callback, not the async task loop.
                auto& editor = AppState::instance().editor();
                const bool added = editor.addEquipmentItem(category, id);
                TOTK_LOG("editor: add %s id=%s added=%d", category.c_str(), id.c_str(), added ? 1 : 0);
                if (!added) {
                    brls::Application::notify("Could not add " + categoryDisplaySingularLower(category) +
                                               " (pouch may be full)");
                    return;
                }
                buildEquipmentGrid(category);
            }),
        brls::TransitionAnimation::NONE);
}

// ---------------------------------------------------------------------------
// Pouch grid (armors/materials/food/devices/key/horses)

size_t TabHostActivity::pouchItemCount(const std::string& category) const {
    auto& editor = AppState::instance().editor();
    if (category == "armors") return editor.armors().size();
    if (category == "horses") return editor.horses().size();
    const auto it = editor.stackItems().find(category);
    return it != editor.stackItems().end() ? it->second.size() : 0;
}

size_t TabHostActivity::pouchCapacity(const std::string& category) const {
    auto& editor = AppState::instance().editor();
    if (category == "armors") return editor.armorCapacity();
    if (category == "horses") return editor.horseCapacity();
    return editor.stackSlotCapacity(category);
}

bool TabHostActivity::pouchAddItem(const std::string& category, const std::string& id) {
    auto& editor = AppState::instance().editor();
    if (category == "armors") return editor.addArmorItem(id);
    if (category == "horses") return editor.addHorseItem(id);
    return editor.addStackItem(category, id);
}

void TabHostActivity::buildPouchGrid(const std::string& category) {
    TOTK_PERF_SCOPE("tabhost.buildPouchGrid.total");
    contentRoot_->clearViews();
    tiles_.clear();

    auto* root = contentRoot_;
    auto& editor = AppState::instance().editor();
    const size_t count = pouchItemCount(category);
    const size_t cap = pouchCapacity(category);
    const bool isFull = count >= cap;

    auto* topBar = new brls::Box();
    topBar->setAxis(brls::Axis::ROW);
    topBar->setWidthPercentage(100);
    topBar->setJustifyContent(brls::JustifyContent::FLEX_END);
    topBar->setAlignItems(brls::AlignItems::CENTER);
    topBar->setMarginBottom(4);
    topBar->setShrink(0);
    topBar->setFocusable(false);

    auto* countLabel = new brls::Label();
    countLabel->setFontSize(16);
    countLabel->setTextColor(nvgRGB(200, 200, 200));
    countLabel->setShrink(0);
    countLabel->setFocusable(false);
    countLabel->setText(std::to_string(count) + " / " + std::to_string(cap));
    topBar->addView(countLabel);
    root->addView(topBar);

    auto* headerRow = new brls::Box();
    headerRow->setAxis(brls::Axis::ROW);
    headerRow->setAlignItems(brls::AlignItems::CENTER);
    headerRow->setWidthPercentage(100);
    headerRow->setMarginBottom(8);
    headerRow->setFocusable(false);

    headerIcon_ = new brls::Image();
    headerIcon_->setScalingType(brls::ImageScalingType::FIT);
    headerIcon_->setWidth(64);
    headerIcon_->setHeight(64);
    headerIcon_->setFocusable(false);
    headerIcon_->setMarginRight(12);
    headerRow->addView(headerIcon_);

    auto* headerText = new brls::Box();
    headerText->setAxis(brls::Axis::COLUMN);
    headerText->setGrow(1.0f);
    headerText->setFocusable(false);

    headerName_ = new brls::Label();
    headerName_->setFontSize(20);
    headerName_->setFocusable(false);
    headerText->addView(headerName_);

    headerSubtitle_ = new brls::Label();
    headerSubtitle_->setFontSize(15);
    headerSubtitle_->setTextColor(nvgRGB(160, 160, 160));
    headerSubtitle_->setFocusable(false);
    headerText->addView(headerSubtitle_);

    headerRow->addView(headerText);
    root->addView(headerRow);

    if (count == 0) {
        headerName_->setText("No " + pouchEmptyPlural(category));
        headerSubtitle_->setText("Select the + tile to add one");
    }

    auto* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1.0f);
    scroll->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    scroll->setFocusable(false);
    scroll->setWidth(1224.0f);
    scroll->setAlignSelf(brls::AlignSelf::FLEX_START);

    auto* content = new brls::Box();
    content->setAxis(brls::Axis::COLUMN);
    content->setWidthPercentage(100);
    content->setFocusable(false);

    tiles_.reserve(count + 1);
    const size_t totalSlots = count + 1;
    for (size_t i = 0; i < totalSlots; ++i) {
        if (i % static_cast<size_t>(kGridColumns) == 0) {
            auto* rowBox = new brls::Box();
            rowBox->setAxis(brls::Axis::ROW);
            rowBox->setFocusable(false);
            content->addView(rowBox);
        }
        auto* rowBox = dynamic_cast<brls::Box*>(content->getChildren().back());
        if (!rowBox) continue;

        auto* tile = new brls::Box();
        tile->setAxis(brls::Axis::COLUMN);
        tile->setAlignItems(brls::AlignItems::CENTER);
        tile->setJustifyContent(brls::JustifyContent::CENTER);
        tile->setWidth(kTileWidth);
        tile->setHeight(kTileHeight);
        tile->setMargins(kTileMargin, kTileMargin, kTileMargin, kTileMargin);
        tile->setPadding(kTileMargin);
        tile->setCornerRadius(kTileCornerRadius);
        tile->setFocusable(true);
        tile->setBackgroundColor(nvgRGBA(255, 255, 255, 12));

        if (i == count) {
            auto* plus = new brls::Label();
            plus->setFocusable(false);
            plus->setFontSize(isFull ? 16.0f : 28.0f);
            plus->setHorizontalAlign(brls::HorizontalAlign::CENTER);
            plus->setText(isFull ? "FULL" : "+");
            plus->setTextColor(isFull ? nvgRGB(220, 90, 90) : nvgRGB(230, 230, 230));
            tile->addView(plus);

            tile->registerAction("Add", brls::BUTTON_A, [this, category, isFull](brls::View*) {
                if (isFull) {
                    brls::Application::notify(pouchDisplaySingular(category) + " pouch is full");
                    return true;
                }
                openPouchAddPicker(category);
                return true;
            });
            tile->addGestureRecognizer(new brls::TapGestureRecognizer(tile));

            rowBox->addView(tile);
            tiles_.push_back(tile);
            continue;
        }

        const auto info = totk::displayInfoForPouchItem(editor, category, static_cast<int>(i));

        auto* badge = new brls::Label();
        badge->setFocusable(false);
        badge->setFontSize(kBadgeFontSize);
        badge->setHorizontalAlign(brls::HorizontalAlign::LEFT);
        badge->setWidth(kTileIconPx);
        badge->setText(info.badge);
        tile->addView(badge);

        auto* image = new brls::Image();
        image->setScalingType(brls::ImageScalingType::FIT);
        image->setWidth(kTileIconPx);
        image->setHeight(kTileIconPx);
        image->setFocusable(false);

        bool iconOk = false;
        for (const auto& path : info.iconPaths) {
            if (IconAtlas::instance().apply(image, path)) {
                iconOk = true;
                break;
            }
        }

        if (iconOk) {
            tile->addView(image);
        } else {
            delete image;
            auto* nameLabel = new brls::Label();
            nameLabel->setFocusable(false);
            nameLabel->setFontSize(13);
            nameLabel->setText(info.name.empty() ? info.id : info.name);
            nameLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
            nameLabel->setWidth(kTileIconPx);
            nameLabel->setIsWrapping(true);
            tile->addView(nameLabel);
        }

        const size_t itemIndex = i;
        tile->registerAction("Select", brls::BUTTON_A, [this, category, itemIndex](brls::View*) {
            brls::Application::pushActivity(new PouchEditActivity(category, itemIndex),
                                             brls::TransitionAnimation::NONE);
            return true;
        });
        tile->addGestureRecognizer(new brls::TapGestureRecognizer(tile));

        rowBox->addView(tile);
        tiles_.push_back(tile);
    }

    scroll->setContentView(content);
    root->addView(scroll);

    wireGridFocus(tiles_, kGridColumns);
    updatePouchHeader(0);
    ensureGridFocusSubscription();

    // See buildEquipmentGrid's matching comment: immediate call closes the
    // ghost-focus gap on ordinary switches, the generation-guarded deferred
    // one is a same-frame-safe fixup for the very first open only. (Not a
    // captured-by-value tile snapshot — a snapshot taken here would still be
    // reachable, and dereferenced, by a stale deferred callback after a
    // later switch had already deleted those exact views.)
    if (!tiles_.empty()) {
        brls::Application::giveFocus(tiles_.front());
    }
    const uint32_t generation = buildGeneration_;
    brls::sync([this, generation]() {
        if (generation != buildGeneration_) return;
        if (!tiles_.empty()) {
            brls::Application::giveFocus(tiles_.front());
        }
    });
}

void TabHostActivity::updatePouchHeader(size_t index) {
    if (index >= pouchItemCount(currentTabId_)) return;
    auto& editor = AppState::instance().editor();
    const auto info = totk::displayInfoForPouchItem(editor, currentTabId_, static_cast<int>(index));
    if (headerName_) headerName_->setText(info.name.empty() ? info.id : info.name);
    if (headerSubtitle_) headerSubtitle_->setText(info.subtitle);
    if (headerIcon_) {
        headerIcon_->clear();
        for (const auto& path : info.iconPaths) {
            if (IconAtlas::instance().apply(headerIcon_, path)) break;
        }
    }
}

void TabHostActivity::openPouchAddPicker(const std::string& category) {
    brls::Application::pushActivity(
        save_editor::ItemPickerActivity::forCategory(
            category, "Add " + pouchDisplaySingular(category),
            [this, category](const std::string& id) {
                if (id.empty()) return;
                const bool added = pouchAddItem(category, id);
                TOTK_LOG("editor: add %s id=%s added=%d", category.c_str(), id.c_str(), added ? 1 : 0);
                if (!added) {
                    brls::Application::notify("Could not add " + pouchDisplaySingular(category) +
                                               " (pouch may be full)");
                    return;
                }
                buildPouchGrid(category);
            }),
        brls::TransitionAnimation::NONE);
}

void TabHostActivity::ensureGridFocusSubscription() {
    if (focusSubscribed_) return;
    focusSubscribed_ = true;
    // One subscription for the activity's whole lifetime — its callback reads
    // currentTabId_ live rather than capturing a category, so it stays
    // correct across every future tab switch without needing to be torn down
    // and re-subscribed each time.
    focusSubscription_ = brls::Application::getGlobalFocusChangeEvent()->subscribe([this](brls::View* view) {
        for (size_t i = 0; i < tiles_.size(); ++i) {
            if (tiles_[i] == view) {
                if (isEquipmentCategory(currentTabId_)) {
                    updateEquipmentHeader(i);
                } else {
                    updatePouchHeader(i);
                }
                return;
            }
        }
    });
}

// ---------------------------------------------------------------------------
// Stats form

void TabHostActivity::buildStatsForm() {
    TOTK_PERF_SCOPE("tabhost.buildStatsForm.total");
    contentRoot_->clearViews();

    auto* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1.0f);
    scroll->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    scroll->setFocusable(false);
    scroll->setWidth(1224.0f);
    scroll->setAlignSelf(brls::AlignSelf::FLEX_START);

    auto* content = new brls::Box();
    content->setAxis(brls::Axis::COLUMN);
    content->setWidthPercentage(100);
    content->setFocusable(false);
    content->setPaddingRight(31.0f);

    versionLabel_ = makeMetaLabel();
    versionLabel_->setText("Game version: -");
    versionLabel_->setMarginBottom(4);
    content->addView(versionLabel_);

    playtimeLabel_ = makeMetaLabel();
    playtimeLabel_->setText("Playtime: -");
    playtimeLabel_->setMarginBottom(12);
    content->addView(playtimeLabel_);

    rupeesCell_ = new totk::ui::BoundedNumericCell();
    rupeesCell_->init(
        "Rupees", 0, 0, totk::GameLimits::kRupeesMax,
        [](long value) { return static_cast<long>(totk::GameLimits::clampRupees(value)); },
        [](long value) { editorStats().rupees = static_cast<uint32_t>(value); }, "0 - 999999");
    addCell(content, rupeesCell_);

    heartsCell_ = new brls::SelectorCell();
    heartsCell_->init(
        "Hearts", heartLabels(), 0,
        [](int selected) { editorStats().maxHearts = heartOptions()[static_cast<size_t>(selected)].quarters; });
    heartsCell_->setWidthPercentage(100);
    heartsCell_->addGestureRecognizer(new brls::TapGestureRecognizer(heartsCell_));
    content->addView(heartsCell_);

    staminaCell_ = new brls::SelectorCell();
    staminaCell_->init("Stamina", staminaLabels(), 0,
                        [](int selected) { editorStats().maxStamina = staminaOptionValue(selected); });
    staminaCell_->setWidthPercentage(100);
    staminaCell_->addGestureRecognizer(new brls::TapGestureRecognizer(staminaCell_));
    content->addView(staminaCell_);

    batteryCell_ = new brls::SelectorCell();
    batteryCell_->init("Battery", batteryLabels(), 0, [](int selected) {
        editorStats().maxBattery = static_cast<float>(batteryOptions()[static_cast<size_t>(selected)].value);
    });
    batteryCell_->setWidthPercentage(100);
    batteryCell_->addGestureRecognizer(new brls::TapGestureRecognizer(batteryCell_));
    content->addView(batteryCell_);

    ponyCell_ = new totk::ui::BoundedNumericCell();
    ponyCell_->init(
        "Pony Points", 0, 0, totk::GameLimits::kPonyPointsMax,
        [](long value) { return static_cast<long>(totk::GameLimits::clampPonyPoints(value)); },
        [](long value) { editorStats().ponyPoints = static_cast<uint32_t>(value); }, "0 - 999999");
    addCell(content, ponyCell_);

    pouchWeaponsCell_ = new totk::ui::BoundedNumericCell();
    pouchWeaponsCell_->init(
        "Weapon Pouch Size", 9, totk::GameLimits::kPouchWeaponsMin, totk::GameLimits::kPouchWeaponsMax,
        [](long value) { return static_cast<long>(totk::GameLimits::clampPouchWeapons(value)); },
        [](long value) { editorStats().pouchWeapons = static_cast<uint32_t>(value); }, "9 - 20");
    addCell(content, pouchWeaponsCell_);

    pouchBowsCell_ = new totk::ui::BoundedNumericCell();
    pouchBowsCell_->init(
        "Bow Pouch Size", 5, totk::GameLimits::kPouchBowsMin, totk::GameLimits::kPouchBowsMax,
        [](long value) { return static_cast<long>(totk::GameLimits::clampPouchBows(value)); },
        [](long value) { editorStats().pouchBows = static_cast<uint32_t>(value); }, "5 - 14");
    addCell(content, pouchBowsCell_);

    pouchShieldsCell_ = new totk::ui::BoundedNumericCell();
    pouchShieldsCell_->init(
        "Shield Pouch Size", 4, totk::GameLimits::kPouchShieldsMin, totk::GameLimits::kPouchShieldsMax,
        [](long value) { return static_cast<long>(totk::GameLimits::clampPouchShields(value)); },
        [](long value) { editorStats().pouchShields = static_cast<uint32_t>(value); }, "4 - 20");
    addCell(content, pouchShieldsCell_);

    constexpr long kCoordMin = -25000;
    constexpr long kCoordMax = 25000;

    posXCell_ = new totk::ui::BoundedNumericCell();
    posXCell_->init(
        "Position X", 0, kCoordMin, kCoordMax, [](long value) { return value; },
        [](long value) { editorStats().savePos.x = static_cast<float>(value); }, "", true);
    addCell(content, posXCell_);

    posYCell_ = new totk::ui::BoundedNumericCell();
    posYCell_->init(
        "Position Y", 0, kCoordMin, kCoordMax, [](long value) { return value; },
        [](long value) { editorStats().savePos.y = static_cast<float>(value); }, "", true);
    addCell(content, posYCell_);

    posZCell_ = new totk::ui::BoundedNumericCell();
    posZCell_->init(
        "Position Z", 0, kCoordMin, kCoordMax, [](long value) { return value; },
        [](long value) { editorStats().savePos.z = static_cast<float>(value); }, "", true);
    addCell(content, posZCell_);

    mapPinsLabel_ = makeMetaLabel();
    mapPinsLabel_->setText("Map pins: 0 / 300");
    mapPinsLabel_->setMarginTop(8);
    mapPinsLabel_->setMarginBottom(8);
    content->addView(mapPinsLabel_);

    auto* removePins = new brls::Button();
    removePins->setText("Remove All Map Pins");
    removePins->registerClickAction([](brls::View*) {
        AppState::instance().editor().removeAllMapPins();
        brls::Application::notify("All map pins removed");
        return true;
    });
    content->addView(removePins);

    scroll->setContentView(content);
    contentRoot_->addView(scroll);

    refreshStatsFromSave();

    // See buildEquipmentGrid's matching comment.
    if (rupeesCell_) {
        brls::Application::giveFocus(rupeesCell_);
    }
    const uint32_t generation = buildGeneration_;
    brls::sync([this, generation]() {
        if (generation != buildGeneration_) return;
        if (rupeesCell_) brls::Application::giveFocus(rupeesCell_);
    });
}

void TabHostActivity::refreshStatsFromSave() {
    auto& editor = AppState::instance().editor();
    if (!editor.isLoaded()) return;

    totk::GameLimits::clampPlayerStats(editor.stats());
    const auto& stats = editor.stats();

    if (versionLabel_) versionLabel_->setText("Game version: " + editor.gameVersion());
    if (playtimeLabel_) playtimeLabel_->setText("Playtime: " + stats.playtime);
    if (rupeesCell_) rupeesCell_->setValue(stats.rupees, true);
    if (heartsCell_) heartsCell_->setSelection(closestHeartSelection(stats.maxHearts), true);
    if (staminaCell_) staminaCell_->setSelection(staminaSelectionForValue(stats.maxStamina), true);
    if (batteryCell_) batteryCell_->setSelection(batterySelectionForValue(stats.maxBattery), true);
    if (ponyCell_) ponyCell_->setValue(stats.ponyPoints, true);
    if (pouchWeaponsCell_) pouchWeaponsCell_->setValue(stats.pouchWeapons, true);
    if (pouchBowsCell_) pouchBowsCell_->setValue(stats.pouchBows, true);
    if (pouchShieldsCell_) pouchShieldsCell_->setValue(stats.pouchShields, true);
    if (posXCell_) posXCell_->setValue(static_cast<long>(stats.savePos.x), true);
    if (posYCell_) posYCell_->setValue(static_cast<long>(stats.savePos.y), true);
    if (posZCell_) posZCell_->setValue(static_cast<long>(stats.savePos.z), true);
    if (mapPinsLabel_) mapPinsLabel_->setText("Map pins: " + std::to_string(stats.mapPinCount) + " / 300");
}

void TabHostActivity::applyStatsToSave() {
    auto& stats = editorStats();
    if (rupeesCell_) stats.rupees = totk::GameLimits::clampRupees(rupeesCell_->getValue());
    if (heartsCell_) stats.maxHearts = heartOptions()[static_cast<size_t>(heartsCell_->getSelection())].quarters;
    if (staminaCell_) stats.maxStamina = staminaOptionValue(staminaCell_->getSelection());
    if (batteryCell_) {
        stats.maxBattery = static_cast<float>(batteryOptions()[static_cast<size_t>(batteryCell_->getSelection())].value);
    }
    if (ponyCell_) stats.ponyPoints = totk::GameLimits::clampPonyPoints(ponyCell_->getValue());
    if (pouchWeaponsCell_) stats.pouchWeapons = totk::GameLimits::clampPouchWeapons(pouchWeaponsCell_->getValue());
    if (pouchBowsCell_) stats.pouchBows = totk::GameLimits::clampPouchBows(pouchBowsCell_->getValue());
    if (pouchShieldsCell_) stats.pouchShields = totk::GameLimits::clampPouchShields(pouchShieldsCell_->getValue());
    if (posXCell_) stats.savePos.x = static_cast<float>(posXCell_->getValue());
    if (posYCell_) stats.savePos.y = static_cast<float>(posYCell_->getValue());
    if (posZCell_) stats.savePos.z = static_cast<float>(posZCell_->getValue());
}

// ---------------------------------------------------------------------------
// Autobuild tab

void TabHostActivity::buildAutobuildTab() {
    TOTK_PERF_SCOPE("tabhost.buildAutobuildTab.total");
    contentRoot_->clearViews();

    auto* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1.0f);
    scroll->setWidthPercentage(100);

    auto* content = new brls::Box();
    content->setAxis(brls::Axis::COLUMN);
    content->setWidthPercentage(100);
    scroll->setContentView(content);
    contentRoot_->addView(scroll);

    auto* browseRow = new brls::Box();
    browseRow->setAxis(brls::Axis::ROW);
    browseRow->setWidthPercentage(100);
    browseRow->setFocusable(true);
    browseRow->setPadding(14, 16, 14, 16);
    browseRow->setCornerRadius(6);
    browseRow->setMarginBottom(14);

    auto* browseLabel = new brls::Label();
    browseLabel->setFocusable(false);
    browseLabel->setGrow(1.0f);
    browseLabel->setFontSize(18);
    browseLabel->setText("Browse HyruleWorks Catalog");
    browseRow->addView(browseLabel);

    browseRow->registerAction(
        "Open", brls::BUTTON_A,
        [](brls::View*) {
            totk::ui::pushAutobuildCatalogActivity();
            return true;
        },
        false, true);
    browseRow->addGestureRecognizer(new brls::TapGestureRecognizer(browseRow));
    content->addView(browseRow);

    auto* hint = new brls::Label();
    hint->setFocusable(false);
    hint->setFontSize(15);
    hint->setTextColor(nvgRGB(160, 160, 160));
    hint->setMarginBottom(10);
    hint->setText("Press X to import a blueprint from the SD card.");
    content->addView(hint);

    auto& editor = AppState::instance().editor();
    const auto favoriteSlots = editor.autobuilderFavoriteSlots();
    const auto& allSlots = editor.autobuilderSlots();

    // Batch-read every Cache-archive thumbnail we might need as a fallback,
    // in one mounted Cache-storage session instead of a separate
    // mount/unmount per row — see SwitchSaveMount::readAutobuildDraftIcons.
    // Favorites imported from HyruleWorks get a locally-saved icon instead
    // (see autobuild_icon_store.hpp) since the console's own render cache is
    // often stale or simply absent for the current Favorites (confirmed
    // 2026-08-19).
    //
    // The cache reader accepts slotValue (Index) and resolves it against the
    // newest lifetime Draft filename. iconKeys/iconDraftPositions stay
    // parallel so the decoded result can still be attached to its physical
    // save-array record.
    std::vector<size_t> iconKeys;
    std::vector<size_t> iconDraftPositions;
    for (const auto& slot : favoriteSlots) {
        if (!slot.occupied || slot.draftPosition >= allSlots.size()) continue;
        const int sv = allSlots[slot.draftPosition].slotValue;
        if (sv < 0) continue;
        iconKeys.push_back(static_cast<size_t>(sv));
        iconDraftPositions.push_back(slot.draftPosition);
    }
    for (const auto& slot : allSlots) {
        if (slot.favorite || slot.blueprint.empty() || slot.slotValue < 0) continue;
        iconKeys.push_back(static_cast<size_t>(slot.slotValue));
        iconDraftPositions.push_back(static_cast<size_t>(slot.index));
    }

    std::vector<std::vector<uint8_t>> iconBytesLocal;
    totk::SwitchSaveMount::readAutobuildDraftIcons(iconKeys, iconBytesLocal);
    std::unordered_map<size_t, size_t> positionToIconIndexLocal;
    for (size_t i = 0; i < iconDraftPositions.size(); ++i) positionToIconIndexLocal[iconDraftPositions[i]] = i;

    // Shared (not per-closure-copied) and outliving this function's stack
    // frame — the loaders built below run later, staggered across future
    // frames via applyAutobuildIconsStaggered, well after buildAutobuildTab()
    // has already returned. Capturing these by reference (the natural thing
    // to do for same-frame use, like the rest of this function) would leave
    // every deferred loader holding a dangling reference to a destroyed
    // local; a shared_ptr keeps exactly one live copy for every closure to
    // share instead of dangling refs or N wasteful per-closure copies.
    auto iconBytes = std::make_shared<std::vector<std::vector<uint8_t>>>(std::move(iconBytesLocal));
    auto positionToIconIndex = std::make_shared<std::unordered_map<size_t, size_t>>(std::move(positionToIconIndexLocal));

    // Sourced the same way for every card, Favorites and History alike: our
    // own SD-side icon (blueprint-content-keyed, always right when we know it —
    // see autobuild_icon_store.hpp) first, falling back to whatever the
    // console's own render cache happens to have.
    auto loadIconFor = [iconBytes, positionToIconIndex](size_t draftPosition,
                                                        const std::vector<uint8_t>& blueprint) {
        return [iconBytes, positionToIconIndex, draftPosition, blueprint](brls::Image* thumb) {
            const auto savedIcon = totk::ui::loadAutobuildIcon(blueprint);
            if (!savedIcon.empty() && totk::loadJpegFromMem(thumb, savedIcon)) return true;
            const auto it = positionToIconIndex->find(draftPosition);
            if (it != positionToIconIndex->end()) return totk::loadPngFromMem(thumb, (*iconBytes)[it->second]);
            return false;
        };
    };

    int occupiedFavorites = 0;
    for (const auto& slot : favoriteSlots) {
        if (slot.occupied) ++occupiedFavorites;
    }

    auto* favoritesHeader = new brls::Header();
    favoritesHeader->setTitle("Favorites (" + std::to_string(occupiedFavorites) + "/" +
                               std::to_string(totk::SaveEditor::kMaxAutobuildFavorites) + ")");
    content->addView(favoritesHeader);

    // Icons are populated after the grid exists, staggered across frames
    // (see applyAutobuildIconsStaggered) rather than decoded synchronously
    // while building the cards — doing all of them in one frame was
    // confirmed to risk a hard crash on real hardware (2026-08-19).
    std::vector<brls::Image*> pendingThumbs;
    std::vector<std::function<bool(brls::Image*)>> pendingLoaders;

    std::vector<totk::ui::AutobuildCardSpec> favoriteCards;
    for (const auto& slot : favoriteSlots) {
        totk::ui::AutobuildCardSpec spec;
        spec.title = "Favorite " + std::to_string(slot.favoriteIndex + 1);
        spec.statusText = slot.occupied ? "Favorite" : "Empty";
        spec.statusColor = slot.occupied ? nvgRGB(120, 200, 120) : nvgRGB(140, 140, 140);
        favoriteCards.push_back(std::move(spec));
    }
    std::vector<brls::Image*> favoriteThumbs;
    totk::ui::buildAutobuildCardGrid(content, kAutobuildGridColumns, favoriteCards, &favoriteThumbs);
    for (size_t i = 0; i < favoriteSlots.size(); ++i) {
        pendingThumbs.push_back(favoriteThumbs[i]);
        pendingLoaders.push_back(favoriteSlots[i].occupied
                                     ? loadIconFor(favoriteSlots[i].draftPosition,
                                                   allSlots[favoriteSlots[i].draftPosition].blueprint)
                                                             : std::function<bool(brls::Image*)>{});
    }

    auto* historyHeader = new brls::Header();
    historyHeader->setTitle("History");
    historyHeader->setMarginTop(14);
    content->addView(historyHeader);

    std::vector<size_t> historyDraftPositions;
    std::vector<totk::ui::AutobuildCardSpec> historyCards;
    for (const auto& slot : allSlots) {
        if (slot.favorite || slot.blueprint.empty()) continue;
        const size_t draftPosition = static_cast<size_t>(slot.index);
        historyDraftPositions.push_back(draftPosition);

        totk::ui::AutobuildCardSpec spec;
        spec.title = "Draft " + std::to_string(slot.index + 1);
        spec.statusText = "Unfavorited";
        spec.statusColor = nvgRGB(150, 160, 210);
        historyCards.push_back(std::move(spec));
    }
    if (historyCards.empty()) {
        auto* emptyLabel = new brls::Label();
        emptyLabel->setFocusable(false);
        emptyLabel->setFontSize(15);
        emptyLabel->setTextColor(nvgRGB(140, 140, 140));
        emptyLabel->setMarginLeft(16);
        emptyLabel->setText("None right now — every draft slot is currently favorited.");
        content->addView(emptyLabel);
    } else {
        std::vector<brls::Image*> historyThumbs;
        totk::ui::buildAutobuildCardGrid(content, kAutobuildGridColumns, historyCards, &historyThumbs);
        for (size_t i = 0; i < historyDraftPositions.size(); ++i) {
            pendingThumbs.push_back(historyThumbs[i]);
            pendingLoaders.push_back(
                loadIconFor(historyDraftPositions[i], allSlots[historyDraftPositions[i]].blueprint));
        }
    }

    // Same immediate + deferred/generation-guarded focus fixup as the
    // equipment/pouch grids above — see buildEquipmentGrid's comment.
    brls::Application::giveFocus(browseRow);
    const uint32_t generation = buildGeneration_;
    brls::sync([this, generation, browseRow]() {
        if (generation != buildGeneration_) return;
        brls::Application::giveFocus(browseRow);
    });

    totk::ui::applyAutobuildIconsStaggered(pendingThumbs, pendingLoaders,
                                            [this, generation]() { return generation == buildGeneration_; });
}

// ---------------------------------------------------------------------------
// About tab

namespace {

brls::Header* addCreditsHeader(brls::Box* content, const std::string& title) {
    auto* header = new brls::Header();
    header->setTitle(title);
    header->setMarginTop(10);
    content->addView(header);
    return header;
}

void addCreditsLine(brls::Box* content, const std::string& text, const std::string& detail = "") {
    auto* row = new brls::Box();
    row->setAxis(brls::Axis::COLUMN);
    row->setWidthPercentage(100);
    row->setFocusable(false);
    row->setPadding(6, 16, 6, 16);

    auto* nameLabel = new brls::Label();
    nameLabel->setFocusable(false);
    nameLabel->setFontSize(16);
    nameLabel->setText(text);
    nameLabel->setIsWrapping(true);
    row->addView(nameLabel);

    if (!detail.empty()) {
        auto* detailLabel = new brls::Label();
        detailLabel->setFocusable(false);
        detailLabel->setFontSize(14);
        detailLabel->setTextColor(nvgRGB(160, 160, 160));
        detailLabel->setText(detail);
        detailLabel->setIsWrapping(true);
        row->addView(detailLabel);
    }

    content->addView(row);
}

}  // namespace

void TabHostActivity::buildCreditsTab() {
    TOTK_PERF_SCOPE("tabhost.buildCreditsTab.total");
    contentRoot_->clearViews();

    // This is a read-only list, so leave the scrolling frame focusable and
    // use natural scrolling: UP/DOWN then scroll the focused frame directly.
    auto* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1.0f);
    scroll->setScrollingBehavior(brls::ScrollingBehavior::NATURAL);
    scroll->setWidth(1224.0f);
    scroll->setAlignSelf(brls::AlignSelf::FLEX_START);

    auto* content = new brls::Box();
    content->setAxis(brls::Axis::COLUMN);
    content->setWidthPercentage(100);
    content->setFocusable(false);
    content->setPaddingRight(31.0f);

    addCreditsHeader(content, "Credits");

    addCreditsHeader(content, "Save Format & Web Editor");
    addCreditsLine(content, "Marc Robledo",
                   "Save parsing/writing logic, web UI design reference, item icon assets, and the "
                   "hashes.csv master variable database this app is built on. "
                   "github.com/marcrobledo/savegame-editors");

    addCreditsHeader(content, "Research & Hash-Crack Credits");
    addCreditsLine(content, "MacSpazzy (SuperSpazzy / McSpazzy)",
                   "Hash-crack and research; original Autobuilder blueprint viewer/editor");
    addCreditsLine(content, "MrCheeze", "Hash-crack and research");
    addCreditsLine(content, "Karlos007", "Hash-crack and research; horse data");
    addCreditsLine(content, "JonJaded, Ozymandias07", "Horse data");
    addCreditsLine(content, "Echocolat, Exincracci, HylianLZ, ApacheThunder, Phil, savage13",
                   "Item research and data contributors");
    addCreditsLine(content, "xiyuesaves", "Filterable item dropdown (web editor)");

    addCreditsHeader(content, "Location Name Data");
    addCreditsLine(content, "zeldamods — objmap-totk",
                   "In-game location display names shown on the save-slot picker");

    addCreditsHeader(content, "Community Build Database");
    addCreditsLine(content, "HyruleWorks", "Community blueprint/build catalog powering the Autobuild browser");

    addCreditsHeader(content, "Third-Party Libraries & Toolchain");
    addCreditsLine(content, "Borealis", "Switch-native UI framework (xfangfang / natinusala / community)");
    addCreditsLine(content, "devkitPro / libnx", "Nintendo Switch homebrew toolchain");

    addCreditsHeader(content, "This Port");
    addCreditsLine(content, "https://github.com/electrotamp");

    addCreditsHeader(content, "App Version");
    addCreditsLine(content, "1.0.0");

    scroll->setContentView(content);
    contentRoot_->addView(scroll);

    brls::Application::giveFocus(scroll);
    const uint32_t generation = buildGeneration_;
    brls::sync([this, generation, scroll]() {
        if (generation != buildGeneration_) return;
        brls::Application::giveFocus(scroll);
    });
}

}  // namespace totk::save_editor
