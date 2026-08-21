#include "util/icon_atlas.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>

#include <borealis/core/application.hpp>
#include <borealis/core/cache_helper.hpp>
#include <borealis/views/image.hpp>
#include <nanovg.h>

#include "util/icon_texture_cache.hpp"
#include "util/perf_trace.hpp"
#include "util/resource_loader.hpp"
#include "util/totk_log.hpp"

namespace totk {

namespace {

std::string atlasCacheKey(const std::string& atlasRelativePath) {
    return "@res/" + atlasRelativePath;
}

uint64_t atlasNowMs() {
#if defined(__SWITCH__)
    return perfNowMs();
#else
    return 0;
#endif
}

}  // namespace

IconAtlas& IconAtlas::instance() {
    static IconAtlas atlas;
    return atlas;
}

bool IconAtlas::ensureLoaded() {
    if (loaded_) return true;
    return loadFromRomfs();
}

bool IconAtlas::loadFromRomfs() {
    entries_.clear();
    atlasPaths_.clear();
    residentSheets_.clear();
    loaded_ = false;

    try {
        const auto text = totk::loadResourceText("assets/icon_atlas/manifest.json");
        const auto json = nlohmann::json::parse(text);
        const auto& atlases = json.at("atlases");
        const auto& entries = json.at("entries");
        entries_.reserve(entries.size());

        for (const auto& [atlasId, atlasInfo] : atlases.items()) {
            (void)atlasId;
            atlasPaths_.push_back(atlasInfo.at("path").get<std::string>());
        }

        for (const auto& [key, value] : entries.items()) {
            const std::string atlasId = value.at("atlas").get<std::string>();
            const std::string atlasPath = atlases.at(atlasId).at("path").get<std::string>();

            IconAtlasRect rect;
            rect.atlasPath = atlasPath;
            rect.x = value.at("x").get<float>();
            rect.y = value.at("y").get<float>();
            rect.w = value.at("w").get<float>();
            rect.h = value.at("h").get<float>();
            entries_.emplace(key, rect);
        }

        loaded_ = !entries_.empty();
        TOTK_LOG("icon_atlas manifest entries=%zu sheets=%zu max_resident=%zu", entries_.size(),
                 atlasPaths_.size(), kMaxResidentSheets);
        return loaded_;
    } catch (const std::exception& ex) {
        TOTK_LOG("icon_atlas load failed: %s", ex.what());
        return false;
    }
}

std::optional<IconAtlasRect> IconAtlas::lookup(const std::string& relativePath) const {
    const auto it = entries_.find(relativePath);
    if (it == entries_.end()) return std::nullopt;
    return it->second;
}

bool IconAtlas::hasEntry(const std::string& relativePath) const {
    if (!loaded_) return false;
    return entries_.find(relativePath) != entries_.end();
}

void IconAtlas::touchSheet(const std::string& atlasRelativePath) {
    const uint64_t now = atlasNowMs();
    for (auto& sheet : residentSheets_) {
        if (sheet.path == atlasRelativePath) {
            sheet.lastUseMs = now;
            return;
        }
    }
}

void IconAtlas::evictLeastRecentSheet() {
    if (residentSheets_.empty()) return;

    auto oldest = std::min_element(
        residentSheets_.begin(), residentSheets_.end(),
        [](const ResidentSheet& a, const ResidentSheet& b) { return a.lastUseMs < b.lastUseMs; });

    TOTK_LOG("icon_atlas sheet evict %s tex=%d", oldest->path.c_str(), oldest->texture);

    if (oldest->texture > 0) {
        brls::TextureCache::instance().markDirty(static_cast<size_t>(oldest->texture));
        nvgDeleteImage(brls::Application::getNVGContext(), oldest->texture);
    }

    residentSheets_.erase(oldest);
}

bool IconAtlas::ensureSheetResident(const std::string& atlasRelativePath) {
    if (atlasRelativePath.empty()) return false;

    const std::string cacheKey = atlasCacheKey(atlasRelativePath);
    if (brls::TextureCache::instance().hasCacheKey(cacheKey)) {
        touchSheet(atlasRelativePath);
        return true;
    }

    while (residentSheets_.size() >= kMaxResidentSheets) {
        evictLeastRecentSheet();
    }

    NVGcontext* vg = brls::Application::getNVGContext();
    if (!vg) return false;

    const std::string filePath = std::string(BRLS_RESOURCES) + atlasRelativePath;
    const int tex = nvgCreateImage(vg, filePath.c_str(), 0);
    if (tex <= 0) {
        TOTK_LOG("icon_atlas sheet load failed %s", filePath.c_str());
        return false;
    }

    brls::TextureCache::instance().addCache(cacheKey, static_cast<size_t>(tex));
    residentSheets_.push_back(ResidentSheet{atlasRelativePath, tex, atlasNowMs()});
    TOTK_LOG("icon_atlas sheet load ok %s tex=%d resident=%zu", atlasRelativePath.c_str(), tex,
             residentSheets_.size());
    return true;
}

void IconAtlas::preloadAtlases() {
    if (!loaded_) return;
    TOTK_LOG("icon_atlas ready sheets=%zu (GPU lazy, max %zu resident)", atlasPaths_.size(),
             kMaxResidentSheets);
}

bool IconAtlas::apply(brls::Image* image, const std::string& relativePath) {
    if (!image || !ensureLoaded()) return false;

    const auto rect = lookup(relativePath);
    if (!rect) return false;

    if (!ensureSheetResident(rect->atlasPath)) return false;

    image->setImageFromAtlasRes(rect->atlasPath, rect->x, rect->y, rect->w, rect->h);
    return image->getOriginalImageWidth() > 0.0f && image->getOriginalImageHeight() > 0.0f;
}

}  // namespace totk
