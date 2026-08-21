#pragma once

#include <functional>
#include <vector>

#include <borealis.hpp>

#include "ui/icon_grid_metrics.hpp"

namespace totk::ui {

enum class IconGridWidthMode {
    TabContent,
    FullWidth,
};

void configureIconGridContainer(brls::Box* gridContent);

std::vector<brls::View*> buildIconGridRows(brls::View* scrollView, brls::Box* gridContent, int slotCount,
                                            int columns, const IconGridMetrics& metrics, IconGridWidthMode widthMode,
                                            const std::function<brls::View*(int slotIndex)>& createTile);

}  // namespace totk::ui
