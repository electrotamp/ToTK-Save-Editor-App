#include "ui/item_picker_activity.hpp"

#include "app/app_state.hpp"
#include "ui/app_theme.hpp"
#include "ui/focus_helpers.hpp"
#include "ui/icon_grid_layout.hpp"
#include "ui/icon_grid_metrics.hpp"
#include "ui/item_database.hpp"
#include "ui/item_icon_tile.hpp"
#include "ui/picker_grid_cache.hpp"
#include "util/icon_texture_cache.hpp"
#include "util/image_loader.hpp"
#include "util/perf_trace.hpp"
#include "util/totk_log.hpp"

namespace totk::ui {

namespace {

// ScrollingFrame::setContentView deletes the previous content view. Detach instead when caching grids.
class ScrollingFrameAccess : public brls::ScrollingFrame {
public:
    static void detachContentView(brls::ScrollingFrame* scroll, brls::View* content) {
        if (!scroll || !content) return;

        auto* frame = static_cast<ScrollingFrameAccess*>(scroll);
        if (frame->contentView != content) return;

        frame->Box::removeView(content, false);
        frame->contentView = nullptr;
    }
};

}  // namespace

ItemPickerActivity::ItemPickerActivity(std::string category, std::string title, std::function<void()> onChanged,
                                       std::function<void(const std::string& itemId)> onSelected)
    : category_(std::move(category)),
      title_(std::move(title)),
      onChanged_(std::move(onChanged)),
      onSelected_(std::move(onSelected)),
      selectOnly_(static_cast<bool>(onSelected_)) {}

ItemPickerActivity* ItemPickerActivity::fusePicker(std::function<void(const std::string& fuseId)> onSelected) {
    return new ItemPickerActivity("", "Fuse Material", nullptr, std::move(onSelected));
}

ItemPickerActivity::~ItemPickerActivity() {
    ++gridImageLoadGeneration_;
    brls::Application::getGlobalFocusChangeEvent()->unsubscribe(focusSubscription_);
}

void ItemPickerActivity::willDisappear(bool resetState) {
    stashGridInCache();
    ++gridImageLoadGeneration_;
    tileBuildIndex_ = static_cast<int>(filteredCatalog_.size());
    unblockPickerInputs();
    TOTK_LOG("ItemPicker %s willDisappear gen=%d", selectOnly_ ? "fuse" : category_.c_str(),
             gridImageLoadGeneration_);
    brls::Activity::willDisappear(resetState);
}

void ItemPickerActivity::unblockPickerInputs() {
    if (!inputsBlocked_) return;
    inputsBlocked_ = false;
    brls::Application::unblockInputs();
}

void ItemPickerActivity::buildFilteredCatalog() {
    filteredCatalog_.clear();
    if (!catalog_) return;

    auto& db = ItemDatabase::instance();
    int skippedCount = 0;

    for (const auto& entry : *catalog_) {
        if (entry.id.empty()) {
            filteredCatalog_.push_back(entry);
            continue;
        }

        const auto iconPaths = selectOnly_ ? db.iconPathCandidatesForId(entry.id)
                                           : db.iconPathCandidates(category_, entry.id);
        bool hasIcon = false;
        for (const auto& path : iconPaths) {
            if (totk::IconTextureCache::instance().hasAsset(path)) {
                hasIcon = true;
                break;
            }
        }
        if (!hasIcon) {
            ++skippedCount;
            continue;
        }
        filteredCatalog_.push_back(entry);
    }

    if (skippedCount > 0) {
        TOTK_LOG("ItemPicker %s filtered out %d entries without romfs icons", category_.c_str(), skippedCount);
        TOTK_PERF("picker.filtered category=%s kept=%zu skipped=%d", category_.c_str(), filteredCatalog_.size(),
                  skippedCount);
    }
}

void ItemPickerActivity::selectIndex(int index) {
    if (filteredCatalog_.empty()) return;

    selectedIndex_ = std::max(0, std::min(index, static_cast<int>(filteredCatalog_.size()) - 1));
    updateDetailPanel(selectedIndex_);
}

void ItemPickerActivity::focusSelectedTile(bool releaseInputsWhenLanded) {
    brls::View* tile = nullptr;
    if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(gridTiles_.size())) {
        tile = gridTiles_[static_cast<size_t>(selectedIndex_)];
    } else if (!gridTiles_.empty()) {
        tile = gridTiles_.front();
    }

    if (!tile) {
        if (releaseInputsWhenLanded) unblockPickerInputs();
        return;
    }

    const int generation = gridImageLoadGeneration_;
    focus::scheduleVisibleFocus(tile, 48, [this, generation, releaseInputsWhenLanded](bool landed) {
        if (generation != gridImageLoadGeneration_) {
            if (releaseInputsWhenLanded) unblockPickerInputs();
            return;
        }
        focus::trace(landed ? "picker.focus_landed" : "picker.focus_failed");
        if (releaseInputsWhenLanded) unblockPickerInputs();
    });
}

void ItemPickerActivity::updateDetailPanel(int index) {
    if (!detailName_ || !detailSubtitle_ || !detailHint_ || !detailIcon_) return;

    const auto metrics = AppState::instance().iconGridMetrics();
    const int detailSize = std::max(72, std::min(metrics.iconSize, 160));
    detailIcon_->setWidth(detailSize);
    detailIcon_->setHeight(detailSize);

    if (index < 0 || index >= static_cast<int>(filteredCatalog_.size())) {
        detailName_->setText("Select an item");
        detailSubtitle_->setText("");
        detailHint_->setText("Navigate the grid below");
        detailIcon_->clear();
        return;
    }

    const auto& entry = filteredCatalog_[static_cast<size_t>(index)];
    const auto iconPaths = selectOnly_ ? ItemDatabase::instance().iconPathCandidatesForId(entry.id)
                                       : ItemDatabase::instance().iconPathCandidates(category_, entry.id);

    detailName_->setText(entry.name);
    detailSubtitle_->setText(entry.id.empty() ? "No material fused" : entry.id);
    detailHint_->setText(selectOnly_ ? "Press A to fuse" : "Press A to add to your pouch");
    if (!totk::loadRomfsImageFirst(detailIcon_, iconPaths)) {
        detailIcon_->clear();
    }
}

void ItemPickerActivity::scrollToTile(brls::View* tile) {
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

bool ItemPickerActivity::tryAddSelectedItem() {
    if (filteredCatalog_.empty()) return false;

    const auto& entry = filteredCatalog_[static_cast<size_t>(selectedIndex_)];
    auto& saveEditor = AppState::instance().editor();
    bool added = false;

    if (category_ == "weapons" || category_ == "bows" || category_ == "shields") {
        added = saveEditor.addEquipmentItem(category_, entry.id);
    } else if (category_ == "armors") {
        added = saveEditor.addArmorItem(entry.id);
    } else if (category_ == "horses") {
        added = saveEditor.addHorseItem(entry.id);
    } else {
        added = saveEditor.addStackItem(category_, entry.id);
    }

    if (!added) {
        brls::Application::notify("Could not add item (pouch may be full)");
        return false;
    }

    const auto onChanged = onChanged_;
    const std::string itemName = entry.name;
    brls::Application::popActivity(brls::TransitionAnimation::FADE, [onChanged, itemName]() {
        brls::sync([onChanged, itemName]() {
            focus::trace("picker.pop.before_onChanged");
            if (onChanged) onChanged();
            focus::trace("picker.pop.after_onChanged");
            brls::Application::notify("Added " + itemName);
            focus::trace("picker.pop.after_notify");
        });
    });
    return true;
}

bool ItemPickerActivity::tryConfirmSelection() {
    if (selectOnly_) {
        if (filteredCatalog_.empty()) return false;

        const auto& entry = filteredCatalog_[static_cast<size_t>(selectedIndex_)];
        const auto onSelected = onSelected_;
        const std::string fuseId = entry.id;
        const std::string itemName = entry.name;

        brls::Application::popActivity(brls::TransitionAnimation::FADE, [onSelected, fuseId, itemName]() {
            brls::sync([onSelected, fuseId, itemName]() {
                if (onSelected) onSelected(fuseId);
                if (fuseId.empty()) {
                    brls::Application::notify("Fusion removed");
                } else {
                    brls::Application::notify("Fused with " + itemName);
                }
            });
        });
        return true;
    }

    return tryAddSelectedItem();
}

void ItemPickerActivity::deferLoadTileImages(int generation, std::function<void()> onComplete) {
    const char* label = selectOnly_ ? "picker.fuse" : category_.c_str();
    batchLoadIconTileImages(tiles_, generation, &gridImageLoadGeneration_, gridContent_, label, 0,
                            [this, generation, onComplete = std::move(onComplete)]() {
                                if (generation == gridImageLoadGeneration_) {
                                    gridImagesLoaded_ = true;
                                }
                                if (onComplete) {
                                    onComplete();
                                }
                            });
}

void ItemPickerActivity::rebindCachedTileCallbacks() {
    const uint64_t rebindStartMs = perfNowMs();
    for (size_t i = 0; i < tiles_.size(); ++i) {
        const int index = static_cast<int>(i);
        tiles_[i]->rebindCallbacks([this, index]() { selectIndex(index); },
                                  [this]() { tryConfirmSelection(); });
    }
    TOTK_PERF("picker.grid_cache rebind ms=%llu tiles=%zu",
              static_cast<unsigned long long>(perfNowMs() - rebindStartMs), tiles_.size());
}

void ItemPickerActivity::finishCachedGridRestore(int loadGeneration, bool imagesLoaded) {
    if (loadGeneration != gridImageLoadGeneration_) return;

    wireIconGridNavigation(gridTiles_, gridColumns_);
    updateDetailPanel(selectedIndex_);

    brls::Application::blockInputs(true);
    inputsBlocked_ = true;

    if (imagesLoaded) {
        TOTK_PERF("picker.grid_cache reuse skip_icon_load");
        focusSelectedTile(true);
    } else {
        deferLoadTileImages(loadGeneration);
        focusSelectedTile(true);
    }
}

void ItemPickerActivity::stashGridInCache() {
    if (!gridContent_ || tiles_.empty() || !gridBuildComplete_ || !gridImagesLoaded_) return;

    const int zoom = AppState::instance().gridZoomPercent();
    gridColumns_ = iconGridColumnsForPicker(gridScroll_, zoom);

    PickerGridCacheEntry entry;
    entry.zoomPercent = zoom;
    entry.gridColumns = gridColumns_;
    entry.catalogSize = filteredCatalog_.size();
    entry.selectOnly = selectOnly_;
    entry.category = category_;
    entry.imagesLoaded = gridImagesLoaded_;
    entry.gridContent = gridContent_;
    entry.gridRows = std::move(gridRows_);
    entry.tiles = std::move(tiles_);
    entry.gridTiles = std::move(gridTiles_);

    TOTK_PERF("picker.grid_cache stash tiles=%zu images=%d", entry.tiles.size(), entry.imagesLoaded ? 1 : 0);

    if (gridScroll_) {
        ScrollingFrameAccess::detachContentView(gridScroll_, gridContent_);
    }

    gridContent_ = nullptr;
    gridRows_.clear();
    tiles_.clear();
    gridTiles_.clear();
    gridBuildComplete_ = false;
    gridImagesLoaded_ = false;

    storePickerGridCache(std::move(entry));
}

bool ItemPickerActivity::tryRestoreCachedGrid(int loadGeneration) {
    if (!gridScroll_ || filteredCatalog_.empty()) return false;

    const int zoom = AppState::instance().gridZoomPercent();
    gridColumns_ = iconGridColumnsForPicker(gridScroll_, zoom);

    PickerGridCacheEntry cached;
    if (!takePickerGridCache(selectOnly_, category_, zoom, gridColumns_, filteredCatalog_.size(), cached)) {
        return false;
    }

    TOTK_PERF("picker.grid_cache restore tiles=%zu images=%d", cached.tiles.size(), cached.imagesLoaded ? 1 : 0);

    gridContent_ = cached.gridContent;
    gridRows_ = std::move(cached.gridRows);
    tiles_ = std::move(cached.tiles);
    gridTiles_ = std::move(cached.gridTiles);
    tileBuildIndex_ = static_cast<int>(filteredCatalog_.size());
    gridBuildComplete_ = true;
    gridImagesLoaded_ = cached.imagesLoaded;

    selectedIndex_ = std::max(0, std::min(selectedIndex_, static_cast<int>(filteredCatalog_.size()) - 1));

    gridScroll_->setContentView(gridContent_);
    rebindCachedTileCallbacks();

    const bool imagesLoaded = cached.imagesLoaded;
    brls::sync([this, loadGeneration, imagesLoaded]() { finishCachedGridRestore(loadGeneration, imagesLoaded); });
    return true;
}

int ItemPickerActivity::buildTileBatch(int generation, int maxTiles) {
    if (generation != gridImageLoadGeneration_) return 0;

    const int count = static_cast<int>(filteredCatalog_.size());
    const auto metrics = iconGridMetricsForZoom(AppState::instance().gridZoomPercent());
    auto& db = ItemDatabase::instance();

    int built = 0;
    while (tileBuildIndex_ < count && built < maxTiles) {
        const int index = tileBuildIndex_++;
        const int rowIndex = index / gridColumns_;
        if (rowIndex < 0 || rowIndex >= static_cast<int>(gridRows_.size())) {
            break;
        }

        const auto& entry = filteredCatalog_[static_cast<size_t>(index)];
        const auto iconPaths =
            selectOnly_ ? db.iconPathCandidatesForId(entry.id) : db.iconPathCandidates(category_, entry.id);

        auto* tile = new ItemIconTile();
        tile->applyLayout(metrics);
        tile->configure(
            "", iconPaths, {}, [this, index]() { selectIndex(index); }, [this]() { tryConfirmSelection(); });
        tiles_.push_back(tile);
        gridTiles_.push_back(tile);
        gridRows_[static_cast<size_t>(rowIndex)]->addView(tile);
        ++built;
    }

    return built;
}

void ItemPickerActivity::finishPickerGridBuild(int generation, bool releaseInputsWhenLanded) {
    if (generation != gridImageLoadGeneration_) return;

    gridBuildComplete_ = true;
    wireIconGridNavigation(gridTiles_, gridColumns_);
    updateDetailPanel(selectedIndex_);
    deferLoadTileImages(generation);
    focusSelectedTile(releaseInputsWhenLanded);
    if (!releaseInputsWhenLanded) {
        unblockPickerInputs();
    }
}

void ItemPickerActivity::scheduleBuildTilesBatch(int generation) {
    brls::sync([this, generation]() {
        if (generation != gridImageLoadGeneration_) return;

        constexpr int kTileBatch = 24;
        const int count = static_cast<int>(filteredCatalog_.size());
        const uint64_t batchStartMs = perfNowMs();
        const int built = buildTileBatch(generation, kTileBatch);

        if (built > 0) {
            TOTK_PERF("picker.build_batch built=%d progress=%d/%d ms=%llu", built, tileBuildIndex_, count,
                      static_cast<unsigned long long>(perfNowMs() - batchStartMs));
        }

        if (tileBuildIndex_ < count) {
            scheduleBuildTilesBatch(generation);
            return;
        }

        finishPickerGridBuild(generation);
    });
}

void ItemPickerActivity::rebuildGrid() {
    if (!gridContent_ || !gridScroll_) return;

    TOTK_PERF_SCOPE("picker.rebuildGrid.layout");

    ++gridImageLoadGeneration_;
    const int loadGeneration = gridImageLoadGeneration_;

    const int zoomPercent = AppState::instance().gridZoomPercent();
    const auto metrics = iconGridMetricsForZoom(zoomPercent);
    gridColumns_ = iconGridColumnsForPicker(gridScroll_, zoomPercent);

    gridBuildComplete_ = false;
    gridImagesLoaded_ = false;
    tileBuildIndex_ = 0;

    const int count = static_cast<int>(filteredCatalog_.size());
    if (count <= 0) {
        updateDetailPanel(-1);
        return;
    }

    selectedIndex_ = std::max(0, std::min(selectedIndex_, count - 1));

    if (tryRestoreCachedGrid(loadGeneration)) {
        return;
    }

    gridContent_->clearViews();
    tiles_.clear();
    gridTiles_.clear();
    gridRows_.clear();

    const float rowWidth = iconGridFullContentWidth(gridScroll_);
    const int rowCount = (count + gridColumns_ - 1) / gridColumns_;
    gridRows_.reserve(static_cast<size_t>(rowCount));
    for (int row = 0; row < rowCount; ++row) {
        auto* rowBox = new brls::Box();
        rowBox->setAxis(brls::Axis::ROW);
        rowBox->setWidth(rowWidth);
        rowBox->setAlignItems(brls::AlignItems::FLEX_START);
        rowBox->setMarginBottom(metrics.tileMargin);
        rowBox->setFocusable(false);
        rowBox->setShrink(0);
        rowBox->setClipsToBounds(false);
        gridContent_->addView(rowBox);
        gridRows_.push_back(rowBox);
    }

    TOTK_LOG("ItemPicker %s deferred build count=%d rows=%d cols=%d", category_.c_str(), count, rowCount, gridColumns_);

    brls::Application::blockInputs(true);
    inputsBlocked_ = true;

    constexpr int kInitialBatch = 24;
    const uint64_t initialBatchStartMs = perfNowMs();
    const int initialBuilt = buildTileBatch(loadGeneration, kInitialBatch);
    if (initialBuilt > 0) {
        TOTK_PERF("picker.build_batch built=%d progress=%d/%d ms=%llu sync=1", initialBuilt, tileBuildIndex_, count,
                  static_cast<unsigned long long>(perfNowMs() - initialBatchStartMs));
        wireIconGridNavigation(gridTiles_, gridColumns_);
        updateDetailPanel(selectedIndex_);
        if (tileBuildIndex_ < count) {
            focusSelectedTile(true);
        }
    }

    if (tileBuildIndex_ < count) {
        scheduleBuildTilesBatch(loadGeneration);
    } else {
        finishPickerGridBuild(loadGeneration, initialBuilt > 0);
    }
}

brls::View* ItemPickerActivity::createContentView() {
    catalog_ = selectOnly_ ? &ItemDatabase::instance().fusibleItems()
                           : &ItemDatabase::instance().pickerItemsForCategory(category_);
    buildFilteredCatalog();

    root_ = new brls::Box();
    root_->setAxis(brls::Axis::COLUMN);
    root_->setWidthPercentage(100);
    root_->setHeightPercentage(100);
    root_->setGrow(1.0f);

    auto* hint = new brls::Label();
    if (filteredCatalog_.empty()) {
        hint->setText(selectOnly_ ? "No fuse materials are available." : "No item catalog is available for this category yet.");
    } else if (selectOnly_) {
        hint->setText(std::to_string(filteredCatalog_.size()) + " materials · A to fuse · B to go back");
    } else {
        hint->setText(std::to_string(filteredCatalog_.size()) + " items · A to add · B to go back");
    }
    hint->setFontSize(15);
    hint->setTextColor(AppTheme::instance().colors().mutedText);
    hint->setMarginBottom(8);
    hint->setShrink(0);
    root_->addView(hint);

    auto* detailPanel = new brls::Box();
    detailPanel->setAxis(brls::Axis::ROW);
    detailPanel->setWidthPercentage(100);
    detailPanel->setAlignItems(brls::AlignItems::CENTER);
    detailPanel->setPadding(12, 12, 12, 12);
    detailPanel->setMarginBottom(8);
    detailPanel->setCornerRadius(6);
    detailPanel->setBackgroundColor(AppTheme::instance().colors().detailPanelBackground);
    detailPanel->setShrink(0);

    detailIcon_ = new brls::Image();
    detailIcon_->setScalingType(brls::ImageScalingType::FIT);
    detailIcon_->setMarginRight(12);
    detailPanel->addView(detailIcon_);

    auto* detailText = new brls::Box();
    detailText->setAxis(brls::Axis::COLUMN);
    detailText->setGrow(1.0f);

    detailName_ = new brls::Label();
    detailName_->setFontSize(20);
    detailName_->setSingleLine(true);
    detailName_->setWidthPercentage(100);
    detailName_->setMarginBottom(4);
    detailText->addView(detailName_);

    detailSubtitle_ = new brls::Label();
    detailSubtitle_->setFontSize(15);
    detailSubtitle_->setSingleLine(true);
    detailSubtitle_->setWidthPercentage(100);
    detailSubtitle_->setTextColor(AppTheme::instance().colors().mutedText);
    detailSubtitle_->setMarginBottom(4);
    detailText->addView(detailSubtitle_);

    detailHint_ = new brls::Label();
    detailHint_->setFontSize(14);
    detailHint_->setSingleLine(true);
    detailHint_->setWidthPercentage(100);
    detailHint_->setTextColor(AppTheme::instance().colors().accentHintText);
    detailText->addView(detailHint_);

    detailPanel->addView(detailText);
    root_->addView(detailPanel);

    if (filteredCatalog_.empty()) {
        auto* frame = new brls::AppletFrame(root_);
        frame->setTitle(selectOnly_ ? title_ : ("Add " + title_));
        updateDetailPanel(-1);
        return frame;
    }

    gridScroll_ = new brls::ScrollingFrame();
    gridScroll_->setGrow(1.0f);
    gridScroll_->setWidthPercentage(100);
    gridScroll_->setClipsToBounds(true);
    gridScroll_->setFocusable(false);
    gridScroll_->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);

    gridContent_ = new brls::Box();
    configureIconGridContainer(gridContent_);

    gridScroll_->setContentView(gridContent_);
    root_->addView(gridScroll_);

    focusSubscription_ = brls::Application::getGlobalFocusChangeEvent()->subscribe([this](brls::View* view) {
        if (!gridContent_ || !view) return;
        for (brls::View* node = view; node; node = node->getParent()) {
            if (node == gridContent_) {
                scrollToTile(view);
                return;
            }
        }
    });

    root_->registerAction(
        selectOnly_ ? "Fuse material" : "Add item", brls::BUTTON_A,
        [this](brls::View*) {
            tryConfirmSelection();
            return true;
        },
        true, false);

    auto* frame = new brls::AppletFrame(root_);
    frame->setTitle(selectOnly_ ? title_ : ("Add " + title_));

    brls::sync([this]() {
        if (!gridScroll_ || !gridContent_) return;
        if (gridScroll_->getWidth() <= 1.0f) {
            brls::sync([this]() { rebuildGrid(); });
            return;
        }
        rebuildGrid();
    });

    return frame;
}

}  // namespace totk::ui
