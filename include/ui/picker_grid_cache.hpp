#pragma once

#include <string>
#include <vector>

#include <borealis.hpp>

namespace totk::ui {

class ItemIconTile;

// Retains built picker grids between activity opens (tiles stay in RAM, not re-bound).
struct PickerGridCacheEntry {
    int zoomPercent = 0;
    int gridColumns = 0;
    size_t catalogSize = 0;
    bool selectOnly = false;
    std::string category;
    bool imagesLoaded = false;

    brls::Box* gridContent = nullptr;
    std::vector<brls::Box*> gridRows;
    std::vector<ItemIconTile*> tiles;
    std::vector<brls::View*> gridTiles;
};

std::string pickerGridCacheKey(bool selectOnly, const std::string& category);
bool takePickerGridCache(bool selectOnly, const std::string& category, int zoomPercent, int gridColumns,
                         size_t catalogSize, PickerGridCacheEntry& out);
void storePickerGridCache(PickerGridCacheEntry entry);
void clearPickerGridCache();

}  // namespace totk::ui
