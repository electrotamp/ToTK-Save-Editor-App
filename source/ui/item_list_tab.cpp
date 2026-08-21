#include "ui/item_list_tab.hpp"

#include "activity/activities.hpp"
#include "app/app_state.hpp"
#include "save/save_editor.hpp"
#include "ui/icon_grid_layout.hpp"
#include "ui/icon_grid_metrics.hpp"
#include "ui/item_database.hpp"
#include "ui/item_detail_header.hpp"
#include "ui/editor_layout.hpp"
#include "ui/focus_helpers.hpp"
#ifndef TOTK_WEAPONS_ONLY_UI
#include "ui/editor_pager_frame.hpp"
#include "ui/item_edit_activity.hpp"
#include "ui/item_picker_activity.hpp"
#endif
#include "ui/item_icon_tile.hpp"
#include "ui/pouch_item_display.hpp"
#include "util/totk_log.hpp"
#include "util/perf_trace.hpp"

namespace totk::ui {

void ItemListTab::logFocusTrace(const char* event) const {
    brls::View* current = brls::Application::getCurrentFocus();
    TOTK_LOG(
        "focus-trace: %s tab=%s epoch=%d restoreEpoch=%d pending=%d overlay=%d rebuild=%d selected=%d tiles=%zu "
        "current=%s",
        event, category_.c_str(), interactionEpoch_, focusRestoreEpoch_, pendingFocusRestore_ ? 1 : 0,
        isUnderOverlayActivity() ? 1 : 0, gridRebuildInProgress_ ? 1 : 0, selectedIndex_, gridTiles_.size(),
        focus::viewLabel(current).c_str());
}

ItemListTab::ItemListTab(const std::string& category, const std::string& title)
    : category_(category), title_(title) {
    this->setAxis(brls::Axis::COLUMN);
    this->setWidthPercentage(100);
    this->setHeightPercentage(100);
    this->setGrow(1.0f);
    this->setFocusable(false);
    this->setPadding(kEditorPagePaddingTop, kEditorPageInset, kEditorPagePaddingBottom, kEditorPageInset);

    focusSubscription_ = brls::Application::getGlobalFocusChangeEvent()->subscribe([this](brls::View* view) {
        if (!isForegroundTab() || !gridContent_ || !view) return;
        for (brls::View* node = view; node; node = node->getParent()) {
            if (node == gridContent_) {
                scrollToTile(view);
                updateFocusDetailForView(view);
                return;
            }
        }
    });

    // Defer grid build until the tab is shown (pager hides inactive pages during XML parse).
#ifdef TOTK_WEAPONS_ONLY_UI
    deferredRebuildPending_ = false;
#else
    deferredRebuildPending_ = true;
#endif

#ifndef TOTK_WEAPONS_ONLY_UI
    gridActionId_ = this->registerAction(
        "Edit item", brls::BUTTON_A,
        [this](brls::View*) {
            if (arrowAddTile_ && brls::Application::getCurrentFocus() == arrowAddTile_) {
                addArrowsFromTab();
                return true;
            }
            if (addTile_ && brls::Application::getCurrentFocus() == addTile_) {
                openItemPicker();
                return true;
            }
            if (gridSlotCount() > 0) {
                handleGridSlotAction(selectedIndex_);
            }
            return true;
        },
        true, false);
#endif
}

ItemListTab::~ItemListTab() {
    brls::Application::getGlobalFocusChangeEvent()->unsubscribe(focusSubscription_);
}

void ItemListTab::willAppear(bool resetState) {
    brls::Box::willAppear(resetState);
    if (!isForegroundTab() || isUnderOverlayActivity()) return;
    brls::sync([this]() {
        if (deferredRebuildPending_ || !gridScroll_) {
            rebuild();
        }
        updateGridLayout();
        ensureTileImagesLoaded();
    });
}

void ItemListTab::willDisappear(bool resetState) {
    brls::Box::willDisappear(resetState);
    pendingFocusRestore_ = false;
    ++interactionEpoch_;
    ++gridImageLoadGeneration_;
}

void ItemListTab::updateCountLabel() {
    if (!countLabel_) return;

    auto& editor = AppState::instance().editor();
    const int count = itemCount();

    if (category_ == "weapons" || category_ == "bows" || category_ == "shields") {
        countLabel_->setText(std::to_string(count) + " / " + std::to_string(editor.equipmentCapacity(category_)));
    } else if (category_ == "horses") {
        const size_t capacity = editor.horseCapacity();
        countLabel_->setText(capacity > 0 ? std::to_string(count) + " / " + std::to_string(capacity)
                                          : std::to_string(count) + " items");
    } else if (category_ == "armors") {
        const size_t capacity = editor.armorCapacity();
        countLabel_->setText(capacity > 0 ? std::to_string(count) + " / " + std::to_string(capacity)
                                          : std::to_string(count) + " items");
    } else {
        const size_t capacity = editor.stackSlotCapacity(category_);
        countLabel_->setText(capacity > 0 ? std::to_string(count) + " / " + std::to_string(capacity)
                                          : std::to_string(count) + " items");
    }
}

void ItemListTab::updateGridLayout() {
    if (!isForegroundTab()) return;
    if (!gridScroll_ || !gridContent_) return;

    const float width = gridScroll_->getWidth();
    if (width <= 1.0f) {
        brls::sync([this]() { updateGridLayout(); });
        return;
    }

    const auto metrics = AppState::instance().iconGridMetrics();
    const int columns = iconGridColumnsForLayout(gridScroll_, gridContent_, metrics.zoomPercent);
    if (columns == gridColumns_ && metrics.zoomPercent == layoutZoomPercent_) {
        return;
    }

    releaseGridFocus();
    rebuildGrid();
    restoreGridFocus();
}

bool ItemListTab::supportsAdd() const {
#ifdef TOTK_WEAPONS_ONLY_UI
    return false;
#else
    return true;
#endif
}

bool ItemListTab::canAddMore() const {
    if (!supportsAdd()) return false;

    auto& editor = AppState::instance().editor();
    const int count = itemCount();
    if (category_ == "horses") {
        return count < static_cast<int>(editor.horseCapacity());
    }
    if (category_ == "weapons" || category_ == "bows" || category_ == "shields") {
        return count < static_cast<int>(editor.equipmentCapacity(category_));
    }
    if (category_ == "armors") {
        return count < static_cast<int>(editor.armorCapacity());
    }
    return count < static_cast<int>(editor.stackSlotCapacity(category_));
}

std::string ItemListTab::addTileCaption() const {
    if (category_ == "weapons") return "Add weapon";
    if (category_ == "bows") return "Add bow";
    if (category_ == "shields") return "Add shield";
    if (category_ == "armors") return "Add armor";
    if (category_ == "materials") return "Add material";
    if (category_ == "food") return "Add food";
    if (category_ == "devices") return "Add device";
    if (category_ == "key") return "Add key item";
    if (category_ == "horses") return "Add horse";
    return "Add item";
}

std::string ItemListTab::databaseCategory() const { return category_; }

int ItemListTab::itemCount() const {
    auto& editor = AppState::instance().editor();
    if (category_ == "armors") return static_cast<int>(editor.armors().size());
    if (category_ == "horses") return static_cast<int>(editor.horses().size());
    if (category_ == "weapons" || category_ == "bows" || category_ == "shields") {
        return static_cast<int>(editor.equipment().at(category_).size());
    }
    return static_cast<int>(editor.stackItems().at(category_).size());
}

void ItemListTab::setFocusDetail(const totk::ItemDisplayInfo& info) {
    if (!focusHeader_) return;

    if (info.name.empty() && info.id.empty()) {
        focusHeader_->setVisibility(brls::Visibility::GONE);
        return;
    }

    focusHeader_->configure(info, 72.0f);
    focusHeader_->setVisibility(brls::Visibility::VISIBLE);
}

void ItemListTab::updateFocusDetailForView(brls::View* view) {
    if (!focusHeader_ || !view) return;

    for (brls::View* node = view; node; node = node->getParent()) {
        if (node == arrowAddTile_) {
            selectIndex(0);
            setFocusDetail(displayInfoForGridSlot(0));
            return;
        }

        if (node == addTile_) {
            const int slot = std::max(0, gridSlotCount() - 1);
            selectIndex(slot);
            setFocusDetail(displayInfoForGridSlot(slot));
            return;
        }

        for (size_t i = 0; i < gridTiles_.size(); ++i) {
            if (gridTiles_[i] != node) continue;

            selectIndex(static_cast<int>(i));
            setFocusDetail(displayInfoForGridSlot(static_cast<int>(i)));
            return;
        }
    }
}

void ItemListTab::selectIndex(int index) {
    const int slots = gridSlotCount();
    if (slots <= 0) {
        selectedIndex_ = 0;
        return;
    }

    selectedIndex_ = std::max(0, std::min(index, slots - 1));
}

void ItemListTab::openItemEditor(int index) { handleGridSlotAction(index); }

void ItemListTab::openItemPicker() {
#ifdef TOTK_WEAPONS_ONLY_UI
    return;
#else
    if (gridRebuildInProgress_) return;

    pendingFocusRestore_ = false;
    ++interactionEpoch_;

    const int count = itemCount();
    const bool canAdd = canAddMore();
    size_t capacity = 0;
    if (category_ == "weapons" || category_ == "bows" || category_ == "shields") {
        capacity = AppState::instance().editor().equipmentCapacity(category_);
    } else if (category_ == "horses") {
        capacity = AppState::instance().editor().horseCapacity();
    } else if (category_ != "armors") {
        capacity = AppState::instance().editor().stackSlotCapacity(category_);
    }
    TOTK_ACTION("open_picker %s count=%d capacity=%zu canAdd=%d", category_.c_str(), count,
                static_cast<size_t>(capacity), canAdd ? 1 : 0);

    if (!canAdd) {
        brls::Application::notify("Pouch is full");
        return;
    }
    brls::Application::pushActivity(new ItemPickerActivity(databaseCategory(), title_, [this]() { onItemAdded(); }),
                                    brls::TransitionAnimation::NONE);
#endif
}

void ItemListTab::scrollToTile(brls::View* tile) {
    if (!tile || !gridScroll_ || !gridContent_) return;

    const brls::Rect scrollBounds = gridScroll_->getFrame();
    const brls::Rect tileBounds = tile->getFrame();
    const float padding = 16.0f;

    float offset = gridScroll_->getContentOffsetY();
    float newOffset = offset;

    if (tileBounds.getMinY() < scrollBounds.getMinY() + padding) {
        newOffset -= (scrollBounds.getMinY() + padding) - tileBounds.getMinY();
    } else if (tileBounds.getMaxY() > scrollBounds.getMaxY() - padding) {
        newOffset += tileBounds.getMaxY() - (scrollBounds.getMaxY() - padding);
    }

    const float maxOffset = std::max(0.0f, gridContent_->getHeight() + 32.0f - gridScroll_->getHeight());
    newOffset = std::max(0.0f, std::min(newOffset, maxOffset));

    if (std::abs(newOffset - offset) > 0.5f) {
        gridScroll_->setContentOffsetY(newOffset, true);
    }
}

void ItemListTab::wireGridNavigation() {
    wireIconGridNavigation(gridTiles_, gridColumns_, {.allowExitLeft = true});
}

void ItemListTab::refresh() { rebuild(); }

void ItemListTab::onItemAdded() {
    TOTK_PERF_SCOPE("grid.onItemAdded");
    const int count = itemCount();
    const int newestItemIndex = std::max(0, count - 1);
    // Bows tab reserves slot 0 for arrows; bow items start at slot 1.
    selectedIndex_ = isBowsTab() ? newestItemIndex + 1 : newestItemIndex;
    TOTK_ACTION("item_added %s selected=%d count=%d", category_.c_str(), selectedIndex_, count);

    brls::sync([this]() {
        logFocusTrace("onItemAdded.begin");
        redirectFocusAwayFromGrid();
        logFocusTrace("onItemAdded.after_redirect_away");
        if (gridContent_ && gridScroll_) {
            updateCountLabel();
            rebuildGrid();
        } else {
            rebuild();
        }
        scheduleFocusRestore();
        logFocusTrace("onItemAdded.after_schedule_restore");
    });
}

void ItemListTab::onItemChanged() {
    brls::sync([this]() {
        if (gridContent_ && gridScroll_) {
            rebuildGrid();
            scheduleFocusRestore();
        }
    });
}

void ItemListTab::onItemDeleted(int deletedIndex) {
    TOTK_ACTION("item_deleted %s index=%d selected=%d count=%d", category_.c_str(), deletedIndex, selectedIndex_,
                itemCount());

    if (deletedIndex < selectedIndex_) {
        selectedIndex_--;
    } else if (deletedIndex == selectedIndex_) {
        selectedIndex_ = std::min(selectedIndex_, std::max(0, itemCount() - 1));
    }

    brls::sync([this]() {
        redirectFocusAwayFromGrid();
        if (gridContent_ && gridScroll_) {
            updateCountLabel();
            rebuildGrid();
        } else {
            rebuild();
        }
        scheduleFocusRestore();
    });
}

void ItemListTab::focusInitialSelection() { activatePageFocus(); }

void ItemListTab::activatePageFocus() {
    if (!isForegroundTab()) {
        logFocusTrace("activatePageFocus.skip_not_foreground");
        return;
    }

    ensureTileImagesLoaded();

    pendingFocusRestore_ = false;
    focusRestoreAttempts_ = 0;

    if (gridRebuildInProgress_ || !gridScroll_ || gridTiles_.empty()) {
        logFocusTrace("activatePageFocus.defer_schedule");
        scheduleFocusRestore();
        return;
    }

    logFocusTrace("activatePageFocus.restore_grid");
    restoreGridFocus();
    if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(gridTiles_.size())) {
        updateFocusDetailForView(gridTiles_[static_cast<size_t>(selectedIndex_)]);
    }
    logFocusTrace("activatePageFocus.done");
}

void ItemListTab::scheduleFocusRestore() {
    pendingFocusRestore_ = true;
    focusRestoreEpoch_ = interactionEpoch_;
    focusRestoreAttempts_ = 0;
    brls::sync([this]() { attemptFocusRestore(); });
}

void ItemListTab::releaseGridFocus() { redirectFocusAwayFromGrid(); }

bool ItemListTab::isForegroundTab() {
#ifdef TOTK_WEAPONS_ONLY_UI
    return true;
#else
    return this->getVisibility() == brls::Visibility::VISIBLE;
#endif
}

bool ItemListTab::isUnderOverlayActivity() const {
#ifdef TOTK_WEAPONS_ONLY_UI
    return false;
#else
    return focus::hasEditorOverlayActivity();
#endif
}

void ItemListTab::redirectFocusAwayFromGrid() {
    if (!isForegroundTab()) return;

    if (focusAnchor_ && focusAnchor_->isFocusable()) {
        focus::focusView(focusAnchor_);
        focus::trace("redirectFocusAwayFromGrid.anchor");
        return;
    }

    redirectFocusToPagerSink();
    focus::trace("redirectFocusAwayFromGrid.pager_sink");
}

void ItemListTab::finishGridRebuild() { gridRebuildInProgress_ = false; }

void ItemListTab::createFocusAnchor() {
    if (focusAnchor_) return;

    focusAnchor_ = new brls::Box();
    focusAnchor_->setPositionType(brls::PositionType::ABSOLUTE);
    focusAnchor_->setPositionTop(0);
    focusAnchor_->setPositionLeft(0);
    focusAnchor_->setVisibility(brls::Visibility::VISIBLE);
    focusAnchor_->setAlpha(0.0f);
    focusAnchor_->setWidth(1);
    focusAnchor_->setHeight(1);
    focusAnchor_->setFocusable(true);
    focusAnchor_->setHideHighlightBackground(true);
    focusAnchor_->setShrink(0);
    focusAnchor_->setCustomNavigationRoute(brls::FocusDirection::UP, focusAnchor_);
    focusAnchor_->setCustomNavigationRoute(brls::FocusDirection::DOWN, focusAnchor_);
    focusAnchor_->setCustomNavigationRoute(brls::FocusDirection::LEFT, focusAnchor_);
    focusAnchor_->setCustomNavigationRoute(brls::FocusDirection::RIGHT, focusAnchor_);
    this->addView(focusAnchor_);
}

void ItemListTab::redirectFocusToPagerSink() {
#ifdef TOTK_WEAPONS_ONLY_UI
    (void)0;
#else
    if (!isForegroundTab()) return;

    for (brls::View* node = this; node; node = node->getParent()) {
        if (auto* pager = dynamic_cast<EditorPagerFrame*>(node)) {
            pager->redirectFocusSink();
            return;
        }
    }
#endif
}

void ItemListTab::completeFocusRestore() {
    if (!pendingFocusRestore_) return;
    if (focusRestoreEpoch_ != interactionEpoch_) {
        logFocusTrace("completeFocusRestore.epoch_mismatch");
        pendingFocusRestore_ = false;
        return;
    }
    if (!isForegroundTab() || isUnderOverlayActivity()) {
        logFocusTrace("completeFocusRestore.wait");
        return;
    }

    pendingFocusRestore_ = false;
    focusRestoreAttempts_ = 0;
    logFocusTrace("completeFocusRestore.before_restore_grid");
    restoreGridFocus();
    if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(gridTiles_.size())) {
        updateFocusDetailForView(gridTiles_[static_cast<size_t>(selectedIndex_)]);
    }
    logFocusTrace("completeFocusRestore.done");
}

void ItemListTab::attemptFocusRestore() {
    if (!pendingFocusRestore_) return;
    if (focusRestoreEpoch_ != interactionEpoch_) {
        logFocusTrace("attemptFocusRestore.epoch_mismatch_abort");
        pendingFocusRestore_ = false;
        return;
    }
    if (!isForegroundTab()) {
        logFocusTrace("attemptFocusRestore.abort_not_foreground");
        pendingFocusRestore_ = false;
        return;
    }
    if (isUnderOverlayActivity()) {
        if (++focusRestoreAttempts_ > 120) {
            logFocusTrace("attemptFocusRestore.give_up_overlay");
            pendingFocusRestore_ = false;
            focusRestoreAttempts_ = 0;
            return;
        }
        if (focusRestoreAttempts_ == 1 || focusRestoreAttempts_ % 20 == 0) {
            logFocusTrace("attemptFocusRestore.wait_overlay");
        }
        brls::sync([this]() { attemptFocusRestore(); });
        return;
    }

    if (gridScroll_ && gridScroll_->getWidth() <= 1.0f) {
        if (++focusRestoreAttempts_ > 120) {
            logFocusTrace("attemptFocusRestore.give_up_width");
            pendingFocusRestore_ = false;
            focusRestoreAttempts_ = 0;
            return;
        }
        brls::sync([this]() { attemptFocusRestore(); });
        return;
    }

    if (gridContent_ && gridTiles_.empty()) {
        const int expectedSlots = gridSlotCount();
        if (expectedSlots > 0) {
            if (++focusRestoreAttempts_ > 120) {
                logFocusTrace("attemptFocusRestore.give_up_empty_grid");
                pendingFocusRestore_ = false;
                focusRestoreAttempts_ = 0;
                return;
            }
            brls::sync([this]() { attemptFocusRestore(); });
            return;
        }
    }

    if (gridRebuildInProgress_) {
        if (++focusRestoreAttempts_ > 120) {
            logFocusTrace("attemptFocusRestore.give_up_rebuild");
            pendingFocusRestore_ = false;
            focusRestoreAttempts_ = 0;
            return;
        }
        if (focusRestoreAttempts_ == 1 || focusRestoreAttempts_ % 20 == 0) {
            logFocusTrace("attemptFocusRestore.wait_rebuild");
        }
        brls::sync([this]() { attemptFocusRestore(); });
        return;
    }

    logFocusTrace("attemptFocusRestore.complete");
    completeFocusRestore();
}

void ItemListTab::restoreGridFocus() {
    if (!isForegroundTab()) {
        logFocusTrace("restoreGridFocus.skip_not_foreground");
        return;
    }

    brls::View* target = nullptr;
    if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(gridTiles_.size())) {
        target = gridTiles_[static_cast<size_t>(selectedIndex_)];
    }

    if (!target || !focus::isEffectivelyVisible(target)) {
        logFocusTrace("restoreGridFocus.skip_bad_target");
        return;
    }

    focus::traceAttempt("restoreGridFocus.before", target);
    focus::focusView(target);
    logFocusTrace("restoreGridFocus.after");
}

void ItemListTab::rebuild() {
    if (!isForegroundTab()) {
        deferredRebuildPending_ = true;
        return;
    }
    deferredRebuildPending_ = false;

    gridRebuildInProgress_ = true;
    if (isForegroundTab()) {
        redirectFocusToPagerSink();
    }
    this->clearViews();
    tiles_.clear();
    gridTiles_.clear();
    gridContent_ = nullptr;
    gridScroll_ = nullptr;
    addTile_ = nullptr;
    focusHeader_ = nullptr;
    countLabel_ = nullptr;
    focusAnchor_ = nullptr;

    auto* topBar = new brls::Box();
    topBar->setAxis(brls::Axis::ROW);
    topBar->setWidthPercentage(100);
    topBar->setJustifyContent(brls::JustifyContent::FLEX_END);
    topBar->setAlignItems(brls::AlignItems::CENTER);
    topBar->setMarginBottom(4);
    topBar->setShrink(0);
    topBar->setFocusable(false);

    countLabel_ = new brls::Label();
    countLabel_->setFontSize(16);
    countLabel_->setTextColor(nvgRGB(200, 200, 200));
    countLabel_->setShrink(0);
    countLabel_->setFocusable(false);
    topBar->addView(countLabel_);
    updateCountLabel();
    this->addView(topBar);

    focusHeader_ = new ItemFocusHeader();
    focusHeader_->setMarginBottom(8);
    focusHeader_->setVisibility(brls::Visibility::GONE);
    this->addView(focusHeader_);

    auto& editor = AppState::instance().editor();
    const int count = itemCount();
    const bool showAddTile = supportsAdd();
    const bool canAdd = canAddMore();
    size_t capacity = 0;
    if (category_ == "weapons" || category_ == "bows" || category_ == "shields") {
        capacity = editor.equipmentCapacity(category_);
    } else if (category_ == "horses") {
        capacity = editor.horseCapacity();
    } else if (category_ != "armors") {
        capacity = editor.stackSlotCapacity(category_);
    }
    const int totalSlots = gridSlotCount();
    if (totalSlots <= 0) {
        auto* empty = new brls::Label();
        empty->setText("No items in this category.");
        empty->setMarginTop(20);
        this->addView(empty);
        TOTK_LOG("ItemListTab %s rows=0", category_.c_str());
        createFocusAnchor();
        finishGridRebuild();
        return;
    }

    selectedIndex_ = std::max(0, std::min(selectedIndex_, totalSlots - 1));

    gridScroll_ = new brls::ScrollingFrame();
    gridScroll_->setGrow(1.0f);
    gridScroll_->setWidthPercentage(100);
    gridScroll_->setClipsToBounds(true);
    gridScroll_->setFocusable(false);
#ifdef TOTK_WEAPONS_ONLY_UI
    gridScroll_->setScrollingBehavior(brls::ScrollingBehavior::NATURAL);
#else
    gridScroll_->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
#endif

    gridContent_ = new brls::Box();
    configureIconGridContainer(gridContent_);

    gridScroll_->setContentView(gridContent_);
    this->addView(gridScroll_);

    brls::sync([this]() {
        if (!gridScroll_ || !gridContent_) return;
        if (gridScroll_->getWidth() <= 1.0f) {
            brls::sync([this]() { rebuildGrid(); });
            return;
        }
        rebuildGrid();
    });

    createFocusAnchor();
}

void ItemListTab::ensureTileImagesLoaded() {
    if (!iconsLoadPending_ || gridRebuildInProgress_) return;
    if (!gridContent_ || tiles_.empty()) {
        iconsLoadPending_ = false;
        return;
    }

    iconsLoadPending_ = false;
    const int generation = gridImageLoadGeneration_;
    brls::sync([this, generation]() { deferLoadTileImages(generation); });
}

void ItemListTab::deferLoadTileImages(int generation) {
    if (!isForegroundTab()) return;
    TOTK_LOG("ItemListTab %s loadImages begin tiles=%zu", category_.c_str(), tiles_.size());
    if (tiles_.empty()) {
        brls::sync([this, generation]() {
            if (generation != gridImageLoadGeneration_) return;
            finishGridRebuild();
            attemptFocusRestore();
        });
        return;
    }

    batchLoadIconTileImages(tiles_, generation, &gridImageLoadGeneration_, gridContent_, category_.c_str(), 0,
                            [this, generation]() {
        if (generation != gridImageLoadGeneration_) return;
        TOTK_LOG("ItemListTab %s loadImages done", category_.c_str());
        finishGridRebuild();
        if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(gridTiles_.size())) {
            updateFocusDetailForView(gridTiles_[static_cast<size_t>(selectedIndex_)]);
        }
        logFocusTrace("loadImages.done.before_focus");
        scheduleFocusRestore();
        logFocusTrace("loadImages.done.after_focus");
    });
}

void ItemListTab::rebuildGrid() {
    if (!gridContent_ || !gridScroll_) return;

    if (!isForegroundTab()) {
        deferredRebuildPending_ = true;
        return;
    }
    deferredRebuildPending_ = false;

    TOTK_PERF_SCOPE("grid.rebuild");
    gridRebuildInProgress_ = true;
    redirectFocusAwayFromGrid();

    ++gridImageLoadGeneration_;
    const int loadGeneration = gridImageLoadGeneration_;

    TOTK_LOG("ItemListTab %s rebuildGrid begin gen=%d", category_.c_str(), loadGeneration);

    const auto metrics = AppState::instance().iconGridMetrics();
    layoutZoomPercent_ = metrics.zoomPercent;
    gridColumns_ = iconGridColumnsForLayout(gridScroll_, gridContent_, metrics.zoomPercent);

    gridContent_->clearViews();
    tiles_.clear();
    gridTiles_.clear();
    addTile_ = nullptr;
    arrowAddTile_ = nullptr;

    auto& editor = AppState::instance().editor();
    const int bowCount = itemCount();
    const bool showAddTile = supportsAdd();
    const bool canAdd = canAddMore();
    const int totalSlots = gridSlotCount();
    if (totalSlots <= 0) {
        finishGridRebuild();
        attemptFocusRestore();
        return;
    }

    selectedIndex_ = std::max(0, std::min(selectedIndex_, totalSlots - 1));

    try {
        gridTiles_ = buildIconGridRows(
            gridScroll_, gridContent_, totalSlots, gridColumns_, metrics, IconGridWidthMode::TabContent,
            [this, &editor, bowCount, showAddTile, metrics, totalSlots](int slotIndex) -> brls::View* {
                if (isBowsTab()) {
                    if (slotIndex == 0) {
                        if (editor.hasArrows()) {
                            const auto info = totk::displayInfoForPouchItem(editor, "arrows", 0);
                            auto* tile = new ItemIconTile();
                            tile->applyLayout(metrics);
                            tile->configure(
                                info.badge, info.iconPaths, info.fuseIconPaths,
                                [this, slotIndex]() {
                                    selectIndex(slotIndex);
                                    updateFocusDetailForView(gridTiles_[static_cast<size_t>(slotIndex)]);
                                },
                                [this, slotIndex]() { handleGridSlotAction(slotIndex); });
                            tiles_.push_back(tile);
                            return tile;
                        }

                        auto* tile = new ItemAddTile();
                        tile->applyLayout(metrics);
                        tile->configure([]() {}, [this]() { addArrowsFromTab(); });
                        arrowAddTile_ = tile;
                        return tile;
                    }

                    const int bowIndex = slotIndex - 1;
                    if (bowIndex < bowCount) {
                        const auto info = totk::displayInfoForPouchItem(editor, category_, bowIndex);
                        auto* tile = new ItemIconTile();
                        tile->applyLayout(metrics);
                        tile->configure(
                            info.badge, info.iconPaths, info.fuseIconPaths,
                            [this, slotIndex]() {
                                selectIndex(slotIndex);
                                updateFocusDetailForView(gridTiles_[static_cast<size_t>(slotIndex)]);
                            },
                            [this, slotIndex]() { handleGridSlotAction(slotIndex); });
                        tiles_.push_back(tile);
                        return tile;
                    }

                    if (showAddTile && slotIndex == totalSlots - 1) {
                        auto* tile = new ItemAddTile();
                        tile->applyLayout(metrics);
                        tile->configure([]() {}, [this]() { openItemPicker(); });
                        addTile_ = tile;
                        return tile;
                    }

                    return nullptr;
                }

                if (slotIndex < bowCount) {
                    const auto info = totk::displayInfoForPouchItem(editor, category_, slotIndex);
                    auto* tile = new ItemIconTile();
                    tile->applyLayout(metrics);
                    tile->configure(
                        info.badge, info.iconPaths, info.fuseIconPaths,
                        [this, slotIndex]() {
                            selectIndex(slotIndex);
                            updateFocusDetailForView(gridTiles_[static_cast<size_t>(slotIndex)]);
                        },
                        [this, slotIndex]() { handleGridSlotAction(slotIndex); });
                    tiles_.push_back(tile);
                    return tile;
                }

                if (showAddTile && slotIndex == bowCount) {
                    auto* tile = new ItemAddTile();
                    tile->applyLayout(metrics);
                    tile->configure([]() {}, [this]() { openItemPicker(); });
                    addTile_ = tile;
                    return tile;
                }

                return nullptr;
            });
    } catch (const std::exception& ex) {
        TOTK_LOG("ItemListTab grid(%s) failed: %s", category_.c_str(), ex.what());
        finishGridRebuild();
        return;
    }

    wireGridNavigation();

    if (!isForegroundTab()) {
        iconsLoadPending_ = true;
        finishGridRebuild();
        return;
    }

    brls::sync([this, loadGeneration]() { deferLoadTileImages(loadGeneration); });

    size_t capacity = 0;
    if (category_ == "weapons" || category_ == "bows" || category_ == "shields") {
        capacity = editor.equipmentCapacity(category_);
    } else if (category_ == "horses") {
        capacity = editor.horseCapacity();
    } else if (category_ != "armors") {
        capacity = editor.stackSlotCapacity(category_);
    }

    TOTK_LOG("ItemListTab %s rows=%d cols=%d zoom=%d capacity=%zu canAdd=%d slots=%d", category_.c_str(), bowCount,
             gridColumns_, metrics.zoomPercent, capacity, canAdd ? 1 : 0, totalSlots);
    TOTK_ACTION("grid_rebuild %s count=%d cols=%d zoom=%d capacity=%zu canAdd=%d slots=%d", category_.c_str(), bowCount,
                gridColumns_, metrics.zoomPercent, capacity, canAdd ? 1 : 0, totalSlots);
}

bool ItemListTab::isBowsTab() const { return category_ == "bows"; }

int ItemListTab::gridSlotCount() const {
    if (!isBowsTab()) {
        return itemCount() + (supportsAdd() ? 1 : 0);
    }
    return 1 + itemCount() + (supportsAdd() ? 1 : 0);
}

totk::ItemDisplayInfo ItemListTab::displayInfoForGridSlot(int slot) const {
    auto& editor = AppState::instance().editor();

    if (isBowsTab()) {
        if (slot == 0) {
            if (editor.hasArrows()) {
                return totk::displayInfoForPouchItem(editor, "arrows", 0);
            }
            totk::ItemDisplayInfo addInfo;
            addInfo.name = "Add arrows";
            return addInfo;
        }

        const int bowIndex = slot - 1;
        if (bowIndex < itemCount()) {
            return totk::displayInfoForPouchItem(editor, category_, bowIndex);
        }

        totk::ItemDisplayInfo addInfo;
        addInfo.name = canAddMore() ? addTileCaption() : addTileCaption() + " (pouch full)";
        return addInfo;
    }

    if (slot < itemCount()) {
        return totk::displayInfoForPouchItem(editor, category_, slot);
    }

    totk::ItemDisplayInfo addInfo;
    addInfo.name = canAddMore() ? addTileCaption() : addTileCaption() + " (pouch full)";
    return addInfo;
}

void ItemListTab::addArrowsFromTab() {
    if (!isBowsTab() || AppState::instance().editor().hasArrows()) return;

    if (!AppState::instance().editor().addArrows()) {
        brls::Application::notify("Could not add arrows");
        return;
    }

    brls::Application::notify("Added arrows");
    selectedIndex_ = 0;
    onItemAdded();
}

void ItemListTab::handleGridSlotAction(int slot) {
    if (gridRebuildInProgress_) return;

#ifdef TOTK_WEAPONS_ONLY_UI
    selectIndex(slot);
    if (slot >= 0 && slot < static_cast<int>(gridTiles_.size())) {
        updateFocusDetailForView(gridTiles_[static_cast<size_t>(slot)]);
    }
    return;
#else
    pendingFocusRestore_ = false;
    ++interactionEpoch_;
    selectIndex(slot);

    if (isBowsTab()) {
        if (slot == 0) {
            if (AppState::instance().editor().hasArrows()) {
                TOTK_ACTION("open_editor arrows index=0");
                brls::Application::pushActivity(new ItemEditActivity(
                    "arrows", 0, [this](int) { onItemAdded(); }));
            } else {
                addArrowsFromTab();
            }
            return;
        }

        const int bowIndex = slot - 1;
        if (bowIndex < itemCount()) {
            TOTK_ACTION("open_editor bows index=%d", bowIndex);
            brls::Application::pushActivity(new ItemEditActivity(
                "bows", bowIndex, [this](int) { onItemAdded(); }));
            return;
        }

        openItemPicker();
        return;
    }

    if (slot < itemCount()) {
        TOTK_ACTION("open_editor %s index=%d", category_.c_str(), slot);
        brls::Application::pushActivity(new ItemEditActivity(
            category_, slot, [this](int deletedIndex) { onItemDeleted(deletedIndex); },
            [this]() { onItemChanged(); }));
        return;
    }

    openItemPicker();
#endif
}

brls::View* ItemListTab::createWeapons() { return new ItemListTab("weapons", "Weapons"); }
brls::View* ItemListTab::createBows() { return new ItemListTab("bows", "Bows"); }
brls::View* ItemListTab::createShields() { return new ItemListTab("shields", "Shields"); }
brls::View* ItemListTab::createArmors() { return new ItemListTab("armors", "Armors"); }
brls::View* ItemListTab::createMaterials() { return new ItemListTab("materials", "Materials"); }
brls::View* ItemListTab::createFood() { return new ItemListTab("food", "Food"); }
brls::View* ItemListTab::createDevices() { return new ItemListTab("devices", "Zonai Devices"); }
brls::View* ItemListTab::createKeyItems() { return new ItemListTab("key", "Key Items"); }
brls::View* ItemListTab::createHorses() { return new ItemListTab("horses", "Horses"); }

}  // namespace totk::ui
