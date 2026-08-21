#include "save_editor/save_editor_item_picker.hpp"

#include "ui/item_database.hpp"
#include "ui/zonai_wisp_overlay.hpp"
#include "util/icon_atlas.hpp"
#include "util/totk_log.hpp"

#include <algorithm>
#include <cctype>

#include <borealis/core/application.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/core/ime.hpp>
#include <borealis/core/touch/tap_gesture.hpp>

namespace totk::save_editor {

namespace {

// Same locked-zoom tile geometry as the main pouch grid (see save_editor_app.cpp)
// but square, since picker tiles show icon only — no badge/name row.
constexpr int kGridColumns = 9;
constexpr float kTileIconPx = 114.0f;
constexpr float kTileSize = 123.0f;
constexpr float kTileMargin = 4.8f;
constexpr float kTileCornerRadius = 7.2f;

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

// Weapon type, from the id prefix — the only picker category with a clean
// sub-type signal in the id itself (bows and shields are one flat prefix
// each, no sub-type marker, so they just sort by name — see applyFilterAndSort).
int weaponTypeBucket(const std::string& id) {
    if (id.rfind("Weapon_Sword_", 0) == 0) return 0;   // Sword
    if (id.rfind("Weapon_Lsword_", 0) == 0) return 1;  // Two-Handed
    if (id.rfind("Weapon_Spear_", 0) == 0) return 2;   // Spear
    return 3;                                          // boss/cut-content weapons — sink to the bottom
}

// Fuse Material source bucket, from the id prefix. Mirrors how the game's own
// material inventory groups things visually; matches the prefix families
// confirmed against items.json's "materials" category (monster drops are by
// far the largest group there, hence bucket 0).
int fuseBucketForId(const std::string& id) {
    if (id.rfind("Item_Enemy_", 0) == 0) return 0;  // Monster Parts
    if (id.rfind("Item_Ore_", 0) == 0) return 1;    // Ore & Gems
    if (id.rfind("Item_Fruit_", 0) == 0 || id.rfind("Item_PlantGet_", 0) == 0 ||
        id.rfind("Item_Mushroom", 0) == 0 || id.rfind("Item_Plant_", 0) == 0 ||
        id.rfind("Item_InsectGet_", 0) == 0 || id.rfind("Animal_Insect", 0) == 0) {
        return 2;  // Plants & Fruit
    }
    if (id.rfind("Item_FishGet_", 0) == 0 || id.rfind("Item_Meat_", 0) == 0) return 3;  // Fish & Meat
    if (id.find("_Roast") != std::string::npos || id.find("_Chilled") != std::string::npos) return 4;  // Food
    if (id.rfind("Weapon_", 0) == 0) return 5;  // Weapon Parts (weapons/bows/shields usable as fuse heads)
    return 6;                                   // anything unmatched (internal/cut-content ids) — sink to the bottom
}

}  // namespace

ItemPickerActivity* ItemPickerActivity::forCategory(std::string category, std::string title,
                                                     std::function<void(const std::string&)> onSelected) {
    const auto& catalog = totk::ItemDatabase::instance().pickerItemsForCategory(category);
    return new ItemPickerActivity(std::move(title), std::move(category),
                                  std::vector<totk::ItemEntry>(catalog.begin(), catalog.end()),
                                  std::move(onSelected));
}

ItemPickerActivity* ItemPickerActivity::forMixedGrid(std::string title, std::vector<totk::ItemEntry> items,
                                                     std::function<void(const std::string&)> onSelected) {
    return new ItemPickerActivity(std::move(title), "", std::move(items), std::move(onSelected));
}

ItemPickerActivity::ItemPickerActivity(std::string title, std::string category, std::vector<totk::ItemEntry> items,
                                       std::function<void(const std::string&)> onSelected)
    : title_(std::move(title)),
      category_(std::move(category)),
      baseItems_(std::move(items)),
      items_(baseItems_),
      onSelected_(std::move(onSelected)) {
    applyFilterAndSort();
}

void ItemPickerActivity::willDisappear(bool resetState) {
    // Same reasoning as WeaponsActivity::willDisappear: unsubscribe before teardown
    // starts, not in the destructor, so a focus reassignment triggered by destroying
    // a focused tile can't re-fire this global event mid-teardown.
    if (focusSubscribed_) {
        brls::Application::getGlobalFocusChangeEvent()->unsubscribe(focusSubscription_);
        focusSubscribed_ = false;
    }
    brls::Activity::willDisappear(resetState);
}

ItemPickerActivity::~ItemPickerActivity() {
    if (focusSubscribed_) {
        brls::Application::getGlobalFocusChangeEvent()->unsubscribe(focusSubscription_);
        focusSubscribed_ = false;
    }
}

void ItemPickerActivity::selectItem(const std::string& id) {
    TOTK_LOG("editor: picker select id=%s", id.c_str());
    // Apply the change BEFORE popping, while this picker (and its icon-grid's worth
    // of atlas-backed Images) is still fully intact. Popping first and mutating in
    // the completion callback let a second grid rebuild land in the same ~90ms
    // window as this picker's own teardown — that compressed overlap was the actual
    // crash, not the picker's teardown by itself (cancelling via B, which does a
    // single rebuild with no mutation, never crashed).
    if (onSelected_) onSelected_(id);
    brls::Application::popActivity(brls::TransitionAnimation::NONE);
}

brls::View* ItemPickerActivity::createContentView() {
    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setWidthPercentage(100);
    root->setHeightPercentage(100);
    root->setGrow(1.0f);
    root->setFocusable(false);
    root->setPadding(12.0f, 40.0f, 28.0f, 40.0f);

    hintLabel_ = new brls::Label();
    hintLabel_->setFocusable(false);
    hintLabel_->setFontSize(15);
    hintLabel_->setTextColor(nvgRGB(160, 160, 160));
    hintLabel_->setMarginBottom(8);
    hintLabel_->setShrink(0);
    root->addView(hintLabel_);

    // Search bar: a real focusable row the cursor can move up into from the grid
    // (see the UP/DOWN wiring in populateGrid), not just a button-hint — pressing
    // A on it opens the same keyboard openSearchKeyboard() always used.
    auto* searchRow = new brls::Box();
    searchRow->setAxis(brls::Axis::ROW);
    searchRow->setWidthPercentage(100);
    searchRow->setAlignItems(brls::AlignItems::CENTER);
    searchRow->setMarginBottom(8);
    searchRow->setShrink(0);
    searchRow->setFocusable(false);

    searchBar_ = new brls::Box();
    searchBar_->setAxis(brls::Axis::ROW);
    searchBar_->setAlignItems(brls::AlignItems::CENTER);
    searchBar_->setGrow(1.0f);
    searchBar_->setFocusable(true);
    searchBar_->setPadding(10.0f, 14.0f, 10.0f, 14.0f);
    searchBar_->setCornerRadius(6.0f);
    searchBar_->setBackgroundColor(nvgRGBA(255, 255, 255, 16));
    searchBar_->registerAction("Search", brls::BUTTON_A, [this](brls::View*) {
        openSearchKeyboard();
        return true;
    });
    searchBar_->addGestureRecognizer(new brls::TapGestureRecognizer(searchBar_));

    searchBarLabel_ = new brls::Label();
    searchBarLabel_->setFocusable(false);
    searchBarLabel_->setFontSize(15);
    searchBarLabel_->setGrow(1.0f);
    searchBar_->addView(searchBarLabel_);
    searchRow->addView(searchBar_);

    root->addView(searchRow);

    refreshHintText();

    // Detail panel: an in-flow themed box (not an overlay/dialog), matching the
    // pre-atlas reference's ItemPickerActivity — tracks the focused tile via the
    // same global-focus-subscribe pattern already proven safe in WeaponsActivity.
    auto* detailPanel = new brls::Box();
    detailPanel->setAxis(brls::Axis::ROW);
    detailPanel->setWidthPercentage(100);
    detailPanel->setAlignItems(brls::AlignItems::CENTER);
    detailPanel->setFocusable(false);
    detailPanel->setPadding(12.0f, 12.0f, 12.0f, 12.0f);
    detailPanel->setMarginBottom(8);
    detailPanel->setCornerRadius(6.0f);
    detailPanel->setBackgroundColor(nvgRGBA(255, 255, 255, 16));
    detailPanel->setShrink(0);

    detailIcon_ = new brls::Image();
    detailIcon_->setScalingType(brls::ImageScalingType::FIT);
    detailIcon_->setWidth(56);
    detailIcon_->setHeight(56);
    detailIcon_->setFocusable(false);
    detailIcon_->setMarginRight(12);
    detailPanel->addView(detailIcon_);

    auto* detailText = new brls::Box();
    detailText->setAxis(brls::Axis::COLUMN);
    detailText->setGrow(1.0f);
    detailText->setFocusable(false);

    detailName_ = new brls::Label();
    detailName_->setFontSize(20);
    detailName_->setSingleLine(true);
    detailName_->setWidthPercentage(100);
    detailName_->setMarginBottom(4);
    detailName_->setFocusable(false);
    detailText->addView(detailName_);

    detailSubtitle_ = new brls::Label();
    detailSubtitle_->setFontSize(15);
    detailSubtitle_->setSingleLine(true);
    detailSubtitle_->setWidthPercentage(100);
    detailSubtitle_->setTextColor(nvgRGB(160, 160, 160));
    detailSubtitle_->setMarginBottom(4);
    detailSubtitle_->setFocusable(false);
    detailText->addView(detailSubtitle_);

    detailHint_ = new brls::Label();
    detailHint_->setFontSize(14);
    detailHint_->setSingleLine(true);
    detailHint_->setWidthPercentage(100);
    detailHint_->setTextColor(nvgRGB(130, 190, 150));
    detailHint_->setFocusable(false);
    detailText->addView(detailHint_);

    detailPanel->addView(detailText);
    root->addView(detailPanel);

    auto* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1.0f);
    // CENTERED gives per-focus-change scroll tracking for free (Borealis's own
    // ScrollingFrame::onChildFocusGained -> updateScrolling()), one row at a time.
    // NATURAL is for free/manual scrolling and doesn't track gamepad focus the same
    // way — combining it with a hand-rolled focus-follow subscription caused both a
    // page-jumpy scroll and a crash (see git history: two independent global focus
    // subscribers, one torn down mid-teardown of the other, use-after-free).
    scroll->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    // See save_editor_app.cpp's matching comment: explicit pixel width (not
    // width:100%+negative margin, which Yoga's stretch sizing ignored) so the
    // frame's own right edge — and thus its scrollbar — lands at x=1264
    // (screenWidth-16), flush with the AppletFrame divider's 30px inset.
    scroll->setWidth(1224.0f);
    scroll->setAlignSelf(brls::AlignSelf::FLEX_START);
    scroll->setContentView(buildIconGrid());
    root->addView(scroll);

    auto* frame = new brls::AppletFrame(root);
    totk::ui::setCenteredHeaderTitle(frame, title_);
    frame_ = frame;
    updateClearSearchAction();
    // Attached to the frame itself, not root — see TabHostActivity's
    // matching comment (tab_host_activity.cpp) for why.
    totk::ui::attachEditorBackground(frame);
    return frame;
}

brls::View* ItemPickerActivity::buildIconGrid() {
    auto* content = new brls::Box();
    content->setAxis(brls::Axis::COLUMN);
    content->setWidthPercentage(100);
    content->setFocusable(false);
    gridContent_ = content;
    populateGrid(content);
    return content;
}

void ItemPickerActivity::populateGrid(brls::Box* content) {
    if (items_.empty()) {
        auto* empty = new brls::Label();
        empty->setFocusable(false);
        empty->setFontSize(15);
        empty->setTextColor(nvgRGB(160, 160, 160));
        empty->setText("No items match \"" + searchQuery_ + "\"");
        empty->setMarginTop(20);
        content->addView(empty);
        tiles_.clear();
        // The previous grid's tiles are gone (this Box was just cleared by the
        // caller) — drop the search bar's DOWN route to them before it dangles.
        if (searchBar_) searchBar_->setCustomNavigationRoute(brls::FocusDirection::DOWN, searchBar_);
        updateDetailForIndex(0);
        return;
    }

    auto& db = totk::ItemDatabase::instance();
    std::vector<brls::View*> tiles;
    tiles.reserve(items_.size());

    brls::Box* rowBox = nullptr;
    for (size_t i = 0; i < items_.size(); ++i) {
        const auto& item = items_[i];

        if (i % static_cast<size_t>(kGridColumns) == 0) {
            rowBox = new brls::Box();
            rowBox->setAxis(brls::Axis::ROW);
            rowBox->setFocusable(false);
            content->addView(rowBox);
        }
        if (!rowBox) continue;

        auto* tile = new brls::Box();
        tile->setAxis(brls::Axis::COLUMN);
        tile->setAlignItems(brls::AlignItems::CENTER);
        tile->setJustifyContent(brls::JustifyContent::CENTER);
        tile->setWidth(kTileSize);
        tile->setHeight(kTileSize);
        tile->setMargins(kTileMargin, kTileMargin, kTileMargin, kTileMargin);
        tile->setPadding(kTileMargin);
        tile->setCornerRadius(kTileCornerRadius);
        tile->setFocusable(true);
        // Nearly-invisible tint, not a solid box — matches the pre-atlas reference's
        // pickers (icon only, no per-tile name/badge; the focused item's name shows
        // in the hint/detail area, not on the tile itself).
        tile->setBackgroundColor(nvgRGBA(255, 255, 255, 12));

        // "None" entry (empty id, e.g. un-fuse) — no icon lookup, just a label.
        if (item.id.empty()) {
            auto* noneLabel = new brls::Label();
            noneLabel->setFocusable(false);
            noneLabel->setFontSize(14);
            noneLabel->setText(item.name.empty() ? "None" : item.name);
            noneLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
            noneLabel->setWidth(kTileIconPx);
            tile->addView(noneLabel);
        } else {
            auto* image = new brls::Image();
            image->setScalingType(brls::ImageScalingType::FIT);
            image->setWidth(kTileIconPx);
            image->setHeight(kTileIconPx);
            image->setFocusable(false);

            // A single category (add-weapon) resolves directly; a mixed grid
            // (fuse material, spanning weapons/bows/shields/materials/food) has
            // no per-entry category on ItemEntry, so resolve it from the id.
            const auto paths = category_.empty() ? db.iconPathCandidatesForId(item.id)
                                                  : db.iconPathCandidates(category_, item.id);
            bool iconOk = false;
            for (const auto& path : paths) {
                if (totk::IconAtlas::instance().apply(image, path)) {
                    iconOk = true;
                    break;
                }
            }

            if (iconOk) {
                tile->addView(image);
            } else {
                // No packed icon for this id (internal/cut-content ids, or ids not
                // yet given source art — e.g. some enemy-drop materials). Fall back
                // to the item's name so the tile is still identifiable and usable
                // instead of rendering blank.
                delete image;
                auto* nameLabel = new brls::Label();
                nameLabel->setFocusable(false);
                nameLabel->setFontSize(13);
                nameLabel->setText(item.name.empty() ? item.id : item.name);
                nameLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
                nameLabel->setWidth(kTileSize - kTileMargin * 2);
                nameLabel->setIsWrapping(true);
                tile->addView(nameLabel);
            }
        }

        const std::string id = item.id;
        tile->registerAction("Select", brls::BUTTON_A, [this, id](brls::View*) {
            selectItem(id);
            return true;
        });
        tile->addGestureRecognizer(new brls::TapGestureRecognizer(tile));

        rowBox->addView(tile);
        tiles.push_back(tile);
    }

    wireGridFocus(tiles, kGridColumns);

    // Route the top row's UP into the search bar (and back down again), instead
    // of wireGridFocus's default self-loop, so the cursor can actually reach it.
    if (searchBar_ && !tiles.empty()) {
        const size_t topRowCount = std::min(tiles.size(), static_cast<size_t>(kGridColumns));
        for (size_t i = 0; i < topRowCount; ++i) {
            tiles[i]->setCustomNavigationRoute(brls::FocusDirection::UP, searchBar_);
        }
        searchBar_->setCustomNavigationRoute(brls::FocusDirection::DOWN, tiles.front());
        searchBar_->setCustomNavigationRoute(brls::FocusDirection::UP, searchBar_);
    }

    tiles_ = tiles;
    updateDetailForIndex(0);

    if (!focusSubscribed_) {
        focusSubscribed_ = true;
        focusSubscription_ = brls::Application::getGlobalFocusChangeEvent()->subscribe([this](brls::View* view) {
            for (size_t i = 0; i < tiles_.size(); ++i) {
                if (tiles_[i] == view) {
                    updateDetailForIndex(i);
                    return;
                }
            }
        });
    }

    if (!tiles.empty()) {
        brls::sync([tiles]() { brls::Application::giveFocus(tiles.front()); });
    }
}

void ItemPickerActivity::rebuildGrid() {
    if (!gridContent_) return;
    gridContent_->clearViews();
    tiles_.clear();
    populateGrid(gridContent_);
    refreshHintText();
}

void ItemPickerActivity::refreshHintText() {
    const bool isFuse = category_.empty();
    if (hintLabel_) {
        std::string text = std::to_string(items_.size());
        if (items_.size() != baseItems_.size()) {
            text += " of " + std::to_string(baseItems_.size());
        }
        text += (isFuse ? " materials · A to fuse · B to go back" : " items · A to add · B to go back");
        hintLabel_->setText(text);
    }
    if (searchBarLabel_) {
        searchBarLabel_->setText(searchQuery_.empty() ? "Search..." : ("\"" + searchQuery_ + "\""));
        searchBarLabel_->setTextColor(searchQuery_.empty() ? nvgRGB(160, 160, 160) : nvgRGB(230, 230, 230));
    }
}

void ItemPickerActivity::updateClearSearchAction() {
    if (!frame_) return;
    const bool shouldShow = !searchQuery_.empty();
    if (shouldShow && !clearSearchRegistered_) {
        clearSearchActionId_ = frame_->registerAction("Clear Search", brls::BUTTON_X, [this](brls::View*) {
            searchQuery_.clear();
            applyFilterAndSort();
            rebuildGrid();
            updateClearSearchAction();
            return true;
        });
        clearSearchRegistered_ = true;
    } else if (!shouldShow && clearSearchRegistered_) {
        frame_->unregisterAction(clearSearchActionId_);
        clearSearchRegistered_ = false;
    }
}

void ItemPickerActivity::applyFilterAndSort() {
    items_.clear();
    items_.reserve(baseItems_.size());

    std::string needle = searchQuery_;
    std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return std::tolower(c); });

    for (const auto& item : baseItems_) {
        // The pinned "None" entry (fuse-removal) always stays visible, even
        // while a search query is active, so clearing a fusion is never blocked
        // by a search that doesn't happen to match "none".
        if (item.id.empty() || needle.empty()) {
            items_.push_back(item);
            continue;
        }
        std::string name = item.name;
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });
        if (name.find(needle) != std::string::npos) items_.push_back(item);
    }

    const bool hasNone = !items_.empty() && items_.front().id.empty();
    const auto sortBegin = items_.begin() + (hasNone ? 1 : 0);

    // Fixed, always-on default order per picker — no user-facing sort control.
    // Fuse Material: grouped by material source (monster parts, ore, plants,
    // fish/meat, food, weapon parts), alphabetical within each group — mirrors
    // how the game's own material inventory reads. Weapons: grouped by type
    // (Sword/Two-Handed/Spear) the same way. Shields/Bows/anything else: name
    // A-Z, since their ids carry no clean sub-type signal to group by (see
    // weaponTypeBucket/fuseBucketForId's comments).
    if (category_.empty()) {
        std::sort(sortBegin, items_.end(), [](const auto& a, const auto& b) {
            const int ba = fuseBucketForId(a.id);
            const int bb = fuseBucketForId(b.id);
            if (ba != bb) return ba < bb;
            return a.name < b.name;
        });
    } else if (category_ == "weapons") {
        std::sort(sortBegin, items_.end(), [](const auto& a, const auto& b) {
            const int ba = weaponTypeBucket(a.id);
            const int bb = weaponTypeBucket(b.id);
            if (ba != bb) return ba < bb;
            return a.name < b.name;
        });
    } else {
        std::sort(sortBegin, items_.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
    }
}

void ItemPickerActivity::openSearchKeyboard() {
    brls::Application::getImeManager()->openForText(
        [this](std::string text) {
            searchQuery_ = std::move(text);
            applyFilterAndSort();
            rebuildGrid();
            updateClearSearchAction();
        },
        "Search " + title_, "Item name", 32, searchQuery_);
}

void ItemPickerActivity::updateDetailForIndex(size_t index) {
    if (!detailName_ || !detailSubtitle_ || !detailHint_ || !detailIcon_) return;

    if (index >= items_.size()) {
        detailName_->setText("Select an item");
        detailSubtitle_->setText("");
        detailHint_->setText("Navigate the grid below");
        detailIcon_->clear();
        return;
    }

    const auto& item = items_[index];
    if (item.id.empty()) {
        detailName_->setText(item.name.empty() ? "None" : item.name);
    } else {
        detailName_->setText(!item.name.empty() ? item.name : totk::ItemDatabase::instance().nameForId(item.id));
    }
    detailSubtitle_->setText(item.id.empty() ? "No material fused" : item.id);
    detailHint_->setText(category_.empty() ? "Press A to fuse" : "Press A to add to your pouch");

    detailIcon_->clear();
    if (!item.id.empty()) {
        auto& db = totk::ItemDatabase::instance();
        const auto paths = category_.empty() ? db.iconPathCandidatesForId(item.id)
                                              : db.iconPathCandidates(category_, item.id);
        for (const auto& path : paths) {
            if (totk::IconAtlas::instance().apply(detailIcon_, path)) break;
        }
    }
}

}  // namespace totk::save_editor
