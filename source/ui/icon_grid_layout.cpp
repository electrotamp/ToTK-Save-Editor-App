#include "ui/icon_grid_layout.hpp"

#include <algorithm>

namespace totk::ui {

namespace {

float rowWidthForMode(brls::View* scrollView, brls::Box* gridContent, IconGridWidthMode widthMode) {
    if (widthMode == IconGridWidthMode::FullWidth) {
        return iconGridFullContentWidth(scrollView);
    }

    return iconGridContentWidth(scrollView, gridContent);
}

}  // namespace

void configureIconGridContainer(brls::Box* gridContent) {
    if (!gridContent) return;

    gridContent->setAxis(brls::Axis::COLUMN);
    gridContent->setWidthPercentage(100);
    gridContent->setPadding(4);
    gridContent->setFocusable(false);
    gridContent->setClipsToBounds(false);
}

std::vector<brls::View*> buildIconGridRows(brls::View* scrollView, brls::Box* gridContent, int slotCount,
                                            int columns, const IconGridMetrics& metrics, IconGridWidthMode widthMode,
                                            const std::function<brls::View*(int slotIndex)>& createTile) {
    std::vector<brls::View*> tiles;
    if (!gridContent || slotCount <= 0 || columns <= 0 || !createTile) {
        return tiles;
    }

    const float rowWidth = rowWidthForMode(scrollView, gridContent, widthMode);

    tiles.reserve(static_cast<size_t>(slotCount));
    for (int slot = 0; slot < slotCount; slot += columns) {
        auto* row = new brls::Box();
        row->setAxis(brls::Axis::ROW);
        row->setWidth(rowWidth);
        row->setAlignItems(brls::AlignItems::FLEX_START);
        row->setMarginBottom(metrics.tileMargin);
        row->setFocusable(false);
        row->setShrink(0);
        row->setClipsToBounds(false);

        for (int col = 0; col < columns && slot + col < slotCount; ++col) {
            const int slotIndex = slot + col;
            brls::View* tile = createTile(slotIndex);
            if (!tile) continue;
            tiles.push_back(tile);
            row->addView(tile);
        }

        gridContent->addView(row);
    }

    return tiles;
}

}  // namespace totk::ui
