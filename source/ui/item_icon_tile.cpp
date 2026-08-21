#include "ui/item_icon_tile.hpp"

#include "app/app_state.hpp"
#include "ui/app_theme.hpp"
#include "util/icon_texture_cache.hpp"
#include "util/image_loader.hpp"
#include "util/perf_trace.hpp"

namespace totk::ui {

namespace {

void applyTileBoxMetrics(brls::Box* tile, const IconGridMetrics& metrics) {
    tile->setWidth(metrics.tileWidth);
    tile->setHeight(metrics.tileHeight);
    tile->setPadding(metrics.tilePadding);
    tile->setMargins(metrics.tileMargin, metrics.tileMargin, metrics.tileMargin, metrics.tileMargin);
    tile->setCornerRadius(metrics.cornerRadius);
    tile->setShrink(0.0f);
    tile->setGrow(0.0f);
}

}  // namespace

void ItemIconTile::applyThemeColors() {
    this->setBackgroundColor(AppTheme::instance().colors().tileBackground);
}

ItemIconTile::ItemIconTile() {
    this->setAxis(brls::Axis::COLUMN);
    this->setFocusable(true);
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setHideHighlightBackground(true);
    this->setClipsToBounds(false);
    applyThemeColors();

    badge_ = new brls::Label();
    badge_->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    badge_->setFocusable(false);
    this->addView(badge_);

    iconWrap_ = new brls::Box();
    iconWrap_->setFocusable(false);
    iconWrap_->setShrink(0.0f);
    iconWrap_->setGrow(0.0f);
    iconWrap_->setClipsToBounds(false);

    baseIcon_ = new brls::Image();
    baseIcon_->setScalingType(brls::ImageScalingType::FIT);
    baseIcon_->setFocusable(false);
    iconWrap_->addView(baseIcon_);

    this->addView(iconWrap_);

    applyLayout(AppState::instance().iconGridMetrics());
}

void ItemIconTile::applyIconLayout(float iconSize) {
    iconSize_ = iconSize;
    iconWrap_->setWidth(iconSize);
    iconWrap_->setHeight(iconSize);
    baseIcon_->setWidth(iconSize);
    baseIcon_->setHeight(iconSize);
}

void ItemIconTile::applyLayout(const IconGridMetrics& metrics) {
    applyTileBoxMetrics(this, metrics);
    badge_->setFontSize(metrics.badgeFontSize);
    badge_->setWidth(metrics.badgeWidth);
    applyIconLayout(static_cast<float>(metrics.iconSize));
}

void ItemIconTile::refreshImageBounds() {
    if (baseIcon_->getTexture() != 0 && baseIcon_->getWidth() > 1.0f && baseIcon_->getHeight() > 1.0f) {
        baseIcon_->invalidate();
    }
}

bool ItemIconTile::needsImageLoad() const {
    for (const auto& path : iconPaths_) {
        if (!totk::IconTextureCache::instance().hasAsset(path)) continue;
        if (loadedIconPath_ == path && baseIcon_->getTexture() != 0) return false;
        return true;
    }
    return baseIcon_->getTexture() == 0 && !loadedIconPath_.empty();
}

void ItemIconTile::loadImages() {
    applyIconLayout(iconSize_);

    if (!needsImageLoad()) {
        refreshImageBounds();
        return;
    }

    if (baseIcon_->getTexture() > 0) {
        baseIcon_->clear();
    }

    if (!totk::loadRomfsImageFirst(baseIcon_, iconPaths_)) {
        loadedIconPath_.clear();
        baseIcon_->clear();
        return;
    }

    loadedIconPath_.clear();
    for (const auto& path : iconPaths_) {
        if (totk::IconTextureCache::instance().isCached(path)) {
            loadedIconPath_ = path;
            break;
        }
    }
    if (loadedIconPath_.empty()) {
        for (const auto& path : iconPaths_) {
            if (totk::IconTextureCache::instance().hasAsset(path)) {
                loadedIconPath_ = path;
                break;
            }
        }
    }

    refreshImageBounds();
}

void ItemIconTile::configure(const std::string& badgeText, const std::vector<std::string>& iconPaths,
                             const std::vector<std::string>& fuseIconPaths, std::function<void()> onFocus,
                             std::function<void()> onActivate) {
    onFocus_ = std::move(onFocus);
    onActivate_ = std::move(onActivate);
    badge_->setText(badgeText);
    badge_->setVisibility(badgeText.empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);

    const bool samePaths = iconPaths_ == iconPaths;
    iconPaths_ = iconPaths;
    (void)fuseIconPaths;

    if (!samePaths) {
        loadedIconPath_.clear();
        baseIcon_->clear();
    }

    if (!clickActionRegistered_) {
        clickActionRegistered_ = true;
        this->registerClickAction([this](brls::View*) {
            if (onActivate_) onActivate_();
            return true;
        });
    }
}

void ItemIconTile::rebindCallbacks(std::function<void()> onFocus, std::function<void()> onActivate) {
    onFocus_ = std::move(onFocus);
    onActivate_ = std::move(onActivate);
}

void ItemIconTile::onFocusGained() {
    brls::Box::onFocusGained();
    if (onFocus_) {
        onFocus_();
    }
}

void wireIconGridNavigation(const std::vector<brls::View*>& tiles, int columns, IconGridNavigationOptions options) {
    if (tiles.empty() || columns <= 0) return;

    const int rows = static_cast<int>((tiles.size() + static_cast<size_t>(columns) - 1) / static_cast<size_t>(columns));

    auto tileAt = [&](int row, int col) -> brls::View* {
        const int index = row * columns + col;
        if (index < 0 || index >= static_cast<int>(tiles.size())) return nullptr;
        return tiles[static_cast<size_t>(index)];
    };

    for (int row = 0; row < rows; ++row) {
        const int colsInRow = std::min(columns, static_cast<int>(tiles.size()) - row * columns);
        for (int col = 0; col < colsInRow; ++col) {
            brls::View* tile = tileAt(row, col);
            if (!tile) continue;

            if (row == 0) {
                tile->setCustomNavigationRoute(brls::FocusDirection::UP, tile);
            } else {
                const int targetCol = std::min(col, columns - 1);
                if (brls::View* up = tileAt(row - 1, targetCol)) {
                    tile->setCustomNavigationRoute(brls::FocusDirection::UP, up);
                }
            }

            if (row + 1 >= rows) {
                tile->setCustomNavigationRoute(brls::FocusDirection::DOWN, tile);
            } else {
                const int nextRowCols = std::min(columns, static_cast<int>(tiles.size()) - (row + 1) * columns);
                const int targetCol = std::min(col, nextRowCols - 1);
                if (brls::View* down = tileAt(row + 1, targetCol)) {
                    tile->setCustomNavigationRoute(brls::FocusDirection::DOWN, down);
                }
            }

            if (col == 0) {
                if (!options.allowExitLeft) {
                    tile->setCustomNavigationRoute(brls::FocusDirection::LEFT, tile);
                }
            } else if (brls::View* left = tileAt(row, col - 1)) {
                tile->setCustomNavigationRoute(brls::FocusDirection::LEFT, left);
            }

            if (col + 1 >= colsInRow) {
                tile->setCustomNavigationRoute(brls::FocusDirection::RIGHT, tile);
            } else if (brls::View* right = tileAt(row, col + 1)) {
                tile->setCustomNavigationRoute(brls::FocusDirection::RIGHT, right);
            }
        }
    }
}

void batchLoadIconTileImages(const std::vector<ItemIconTile*>& tiles, int generation, int* activeGeneration,
                             brls::Box* gridContent, const char* label, size_t startIndex,
                             std::function<void()> onComplete) {
    if (!activeGeneration || generation != *activeGeneration) return;

    if (startIndex == 0) {
        beginImageBatch(label, generation);
    }

    constexpr size_t kColdBatch = 12;
    constexpr size_t kWarmBatch = 48;
    size_t batchSize = kWarmBatch;
    for (size_t i = startIndex; i < tiles.size(); ++i) {
        if (tiles[i]->needsImageLoad()) {
            batchSize = kColdBatch;
            break;
        }
    }
    const size_t end = std::min(startIndex + batchSize, tiles.size());
    const uint64_t batchStartMs = perfNowMs();

    for (size_t i = startIndex; i < end; ++i) {
        tiles[i]->loadImages();
        tiles[i]->refreshImageBounds();
    }

    if (end < tiles.size()) {
        perfLog("icon_batch_chunk label=%s gen=%d range=%zu-%zu ms=%llu", label ? label : "icons", generation,
                startIndex, end - 1, static_cast<unsigned long long>(perfNowMs() - batchStartMs));
    }

    if (gridContent) {
        gridContent->invalidate();
    }

    if (end < tiles.size()) {
        brls::sync([tiles, generation, activeGeneration, gridContent, label, end, onComplete]() {
            batchLoadIconTileImages(tiles, generation, activeGeneration, gridContent, label, end, onComplete);
        });
        return;
    }

    brls::sync([tiles, generation, activeGeneration, gridContent, label, onComplete]() {
        if (!activeGeneration || generation != *activeGeneration) return;
        for (auto* tile : tiles) {
            tile->refreshImageBounds();
        }
        if (gridContent) {
            gridContent->invalidate();
        }
        endImageBatch(static_cast<int>(tiles.size()), generation);
        if (onComplete) {
            onComplete();
        }
    });
}

ItemAddTile::ItemAddTile() {
    this->setAxis(brls::Axis::COLUMN);
    this->setFocusable(true);
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setJustifyContent(brls::JustifyContent::CENTER);
    applyThemeColors();
    this->setHideHighlightBackground(true);

    plus_ = new brls::Label();
    plus_->setText("+");
    plus_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    plus_->setFocusable(false);
    this->addView(plus_);

    applyLayout(AppState::instance().iconGridMetrics());
}

void ItemAddTile::applyLayout(const IconGridMetrics& metrics) {
    applyTileBoxMetrics(this, metrics);
    plus_->setFontSize(metrics.plusFontSize);
}

void ItemAddTile::applyThemeColors() {
    this->setBackgroundColor(AppTheme::instance().colors().tileBackground);
}

void ItemAddTile::configure(std::function<void()> onFocus, std::function<void()> onActivate) {
    onFocus_ = std::move(onFocus);
    onActivate_ = std::move(onActivate);

    this->registerClickAction([this](brls::View*) {
        if (onActivate_) onActivate_();
        return true;
    });
}

void ItemAddTile::onFocusGained() {
    brls::Box::onFocusGained();
    if (onFocus_) {
        onFocus_();
    }
}

}  // namespace totk::ui
