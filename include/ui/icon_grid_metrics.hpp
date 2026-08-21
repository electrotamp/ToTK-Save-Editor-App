#pragma once

namespace brls {
class Box;
class View;
}

namespace totk::ui {

struct IconGridMetrics {
    int zoomPercent = 100;
    int iconSize = 128;
    int tileWidth = 136;
    int tileHeight = 148;
    int tilePitchX = 144;
    int tilePitchY = 160;
    int badgeFontSize = 14;
    int badgeWidth = 128;
    int plusFontSize = 52;
    float tilePadding = 4.0f;
    float tileMargin = 4.0f;
    float cornerRadius = 6.0f;
};

constexpr int kIconGridZoomMin = 120;
constexpr int kIconGridZoomMax = 120;
constexpr int kIconGridZoomStep = 20;
constexpr int kIconGridZoomDefault = 120;
constexpr int kIconGridZoomBaseline = 200;
constexpr int kIconSourcePixels = 190;
constexpr int kIconGridMaxColumns = 12;

// Full-width layout: no sidebar column.
constexpr float kIconGridSidebarWidth = 0.0f;
constexpr float kIconGridAppletSidePadding = 40.0f;
constexpr float kIconGridScreenWidth = 1280.0f;
constexpr float kIconGridContentPaddingX = 8.0f;
constexpr float kIconGridFooterHintReserve = 0.0f;

IconGridMetrics iconGridMetricsForZoom(int zoomPercent);
int clampIconGridZoom(int zoomPercent);
int iconDisplaySizeForZoom(int zoomPercent);
float iconGridNominalContentWidth();
float iconGridFullContentWidth(brls::View* scrollView);
float iconGridContentWidth(brls::View* scrollView, brls::Box* contentView = nullptr);
int iconGridColumnsForZoom(int zoomPercent);
int iconGridColumnsForPicker(brls::View* scrollView, int zoomPercent);
int iconGridColumnsForWidth(float availableWidth, const IconGridMetrics& metrics);
int iconGridColumnsForLayout(brls::View* scrollView, brls::Box* contentView, int zoomPercent);

}  // namespace totk::ui
