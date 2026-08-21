#include "ui/picker_grid_cache.hpp"

#include <unordered_map>

namespace totk::ui {

namespace {

std::unordered_map<std::string, PickerGridCacheEntry>& cacheMap() {
    static std::unordered_map<std::string, PickerGridCacheEntry> map;
    return map;
}

void destroyEntry(PickerGridCacheEntry& entry) {
    if (entry.gridContent) {
        entry.gridContent->clearViews();
        delete entry.gridContent;
        entry.gridContent = nullptr;
    }
    entry.gridRows.clear();
    entry.tiles.clear();
    entry.gridTiles.clear();
}

}  // namespace

std::string pickerGridCacheKey(bool selectOnly, const std::string& category) {
    return selectOnly ? std::string("fuse") : category;
}

bool takePickerGridCache(bool selectOnly, const std::string& category, int zoomPercent, int gridColumns,
                         size_t catalogSize, PickerGridCacheEntry& out) {
    const std::string key = pickerGridCacheKey(selectOnly, category);
    auto it = cacheMap().find(key);
    if (it == cacheMap().end()) return false;

    PickerGridCacheEntry& entry = it->second;
    if (entry.zoomPercent != zoomPercent || entry.gridColumns != gridColumns || entry.catalogSize != catalogSize ||
        entry.selectOnly != selectOnly || entry.category != category || !entry.gridContent || entry.tiles.empty()) {
        return false;
    }

    out = std::move(entry);
    cacheMap().erase(it);
    return true;
}

void storePickerGridCache(PickerGridCacheEntry entry) {
    if (!entry.gridContent || entry.tiles.empty()) {
        destroyEntry(entry);
        return;
    }

    const std::string key = pickerGridCacheKey(entry.selectOnly, entry.category);
    auto& map = cacheMap();
    auto it = map.find(key);
    if (it != map.end()) {
        destroyEntry(it->second);
    }
    map[key] = std::move(entry);
}

void clearPickerGridCache() {
    for (auto& [_, entry] : cacheMap()) {
        destroyEntry(entry);
    }
    cacheMap().clear();
}

}  // namespace totk::ui
