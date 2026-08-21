#include "ui/icon_grid_metrics.hpp"

#include <borealis.hpp>

#include <algorithm>
#include <cmath>

namespace totk::ui {

int clampIconGridZoom(int zoomPercent) {
    const int step = kIconGridZoomStep;
    const int rounded = ((zoomPercent + step / 2) / step) * step;
    return std::max(kIconGridZoomMin, std::min(rounded, kIconGridZoomMax));
}

int iconDisplaySizeForZoom(int zoomPercent) {
    const int zoom = clampIconGridZoom(zoomPercent);
    return (kIconSourcePixels * zoom + kIconGridZoomBaseline / 2) / kIconGridZoomBaseline;
}

IconGridMetrics iconGridMetricsForZoom(int zoomPercent) {
    IconGridMetrics metrics;
    metrics.zoomPercent = clampIconGridZoom(zoomPercent);
    metrics.iconSize = iconDisplaySizeForZoom(metrics.zoomPercent);

    const float scale = static_cast<float>(metrics.zoomPercent) / 100.0f;
    metrics.tilePadding = std::max(2.0f, 4.0f * scale);
    metrics.tileMargin = metrics.tilePadding;
    metrics.badgeFontSize = std::max(10, static_cast<int>(std::lround(14.0f * scale)));
    metrics.plusFontSize = std::max(24, static_cast<int>(std::lround(52.0f * scale)));
    metrics.cornerRadius = std::max(4.0f, 6.0f * scale);

    metrics.tileWidth = metrics.iconSize + static_cast<int>(metrics.tilePadding * 2.0f);
    metrics.tileHeight =
        metrics.badgeFontSize + 2 + metrics.iconSize + static_cast<int>(metrics.tilePadding * 2.0f);
    metrics.badgeWidth = metrics.tileWidth - static_cast<int>(metrics.tilePadding * 2.0f);
    metrics.tilePitchX = metrics.tileWidth + static_cast<int>(metrics.tileMargin * 2.0f);
    metrics.tilePitchY = metrics.tileHeight + static_cast<int>(metrics.tileMargin * 2.0f);
    return metrics;
}

float iconGridNominalContentWidth() {
    return kIconGridScreenWidth - (kIconGridAppletSidePadding * 2.0f) - kIconGridSidebarWidth -
           kIconGridContentPaddingX;
}

float iconGridFullContentWidth(brls::View* scrollView) {
    float width = 0.0f;
    if (scrollView) {
        width = scrollView->getFrame().getWidth();
        if (width <= 1.0f) {
            width = scrollView->getWidth();
        }
    }
    if (width <= 1.0f) {
        width = kIconGridScreenWidth - (kIconGridAppletSidePadding * 2.0f);
    }
    return std::max(0.0f, width - kIconGridContentPaddingX);
}

float iconGridContentWidth(brls::View* scrollView, brls::Box* contentView) {
    float width = 0.0f;

    if (scrollView) {
        width = scrollView->getFrame().getWidth();
        if (width <= 1.0f) {
            width = scrollView->getWidth();
        }
    }

    if (width <= 1.0f) {
        width = iconGridNominalContentWidth();
    } else if (contentView) {
        (void)contentView;
        width -= kIconGridContentPaddingX;
    }

    // Never assume more width than the tab content area can provide.
    width = std::min(width, iconGridNominalContentWidth());
    return std::max(0.0f, width);
}

int iconGridColumnsForZoom(int zoomPercent) {
    switch (clampIconGridZoom(zoomPercent)) {
        case 60:
            return 9;
        case 80:
            return 7;
        case 100:
            return 5;
        case 120:
            return 4;
        case 140:
            return 4;
        case 160:
            return 3;
        case 180:
            return 3;
        case 200:
            return 2;
        default:
            break;
    }

    const auto metrics = iconGridMetricsForZoom(zoomPercent);
    return iconGridColumnsForWidth(iconGridNominalContentWidth(), metrics);
}

int iconGridColumnsForWidth(float availableWidth, const IconGridMetrics& metrics) {
    if (availableWidth <= 0.0f || metrics.tilePitchX <= 0) {
        return 1;
    }

    const int columns = static_cast<int>(std::floor(availableWidth / static_cast<float>(metrics.tilePitchX)));
    return std::max(1, std::min(columns, kIconGridMaxColumns));
}

int iconGridColumnsForPicker(brls::View* scrollView, int zoomPercent) {
    const auto metrics = iconGridMetricsForZoom(zoomPercent);
    return iconGridColumnsForWidth(iconGridFullContentWidth(scrollView), metrics);
}

int iconGridColumnsForLayout(brls::View* scrollView, brls::Box* contentView, int zoomPercent) {
    const auto metrics = iconGridMetricsForZoom(zoomPercent);
    return iconGridColumnsForWidth(iconGridContentWidth(scrollView, contentView), metrics);
}

}  // namespace totk::ui
