#include "util/icon_texture_cache.hpp"

#include <cstdio>
#include <memory>
#include <vector>

#include <borealis/core/application.hpp>
#include <borealis/core/cache_helper.hpp>
#include <borealis/core/thread.hpp>

#include "util/perf_trace.hpp"
#include "util/totk_log.hpp"
#ifdef TOTK_ICON_ATLAS
#include "util/icon_atlas.hpp"
#endif

namespace totk {

IconTextureCache& IconTextureCache::instance() {
    static IconTextureCache cache;
    return cache;
}

std::string IconTextureCache::cacheKey(const std::string& relativePath) {
    return "@res/" + relativePath;
}

void IconTextureCache::ensureCapacity(size_t expectedTextures) {
    brls::TextureCache::instance().cache.setCapacity(expectedTextures);
}

bool IconTextureCache::hasAsset(const std::string& relativePath) const {
    if (relativePath.empty()) return false;

#ifdef TOTK_ICON_ATLAS
    if (IconAtlas::instance().ensureLoaded() && IconAtlas::instance().hasEntry(relativePath)) {
        return true;
    }
#endif

    const std::string path = "romfs:/" + relativePath;
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    std::fclose(file);
    return true;
}

bool IconTextureCache::isCached(const std::string& relativePath) const {
    if (relativePath.empty()) return false;
    return brls::TextureCache::instance().hasCacheKey(cacheKey(relativePath));
}

bool IconTextureCache::apply(brls::Image* image, const std::string& relativePath) {
    if (!image || relativePath.empty()) return false;

#ifdef TOTK_ICON_ATLAS
    if (IconAtlas::instance().ensureLoaded()) {
#ifdef TOTK_NXLINK_DEBUG
        const uint64_t atlasStartMs = perfNowMs();
#endif
        if (IconAtlas::instance().apply(image, relativePath)) {
#ifdef TOTK_NXLINK_DEBUG
            TOTK_PERF("icon_atlas.apply path=%s ms=%llu", relativePath.c_str(),
                      static_cast<unsigned long long>(perfNowMs() - atlasStartMs));
#endif
            return true;
        }
    }
#endif

    const std::string key = cacheKey(relativePath);
    const bool cached = brls::TextureCache::instance().hasCacheKey(key);
    const uint64_t startMs = perfNowMs();

    if (cached) {
        image->setImageFromRes(relativePath);
        const bool ok =
            image->getOriginalImageWidth() > 0.0f && image->getOriginalImageHeight() > 0.0f;
        if (ok) {
            return true;
        }
    }

    if (image->getTexture() > 0) {
        image->clear();
    }
    image->setImageFromRes(relativePath);

    const bool ok = image->getOriginalImageWidth() > 0.0f && image->getOriginalImageHeight() > 0.0f;
    if (ok && !cached) {
        textureCount_++;
        recordImageLoad(true, 0, perfNowMs() - startMs, relativePath.c_str());
    }
    return ok;
}

bool IconTextureCache::applyFirst(brls::Image* image, const std::vector<std::string>& relativePaths) {
#ifdef TOTK_ICON_ATLAS
    if (IconAtlas::instance().ensureLoaded()) {
        for (const auto& path : relativePaths) {
            if (IconAtlas::instance().apply(image, path)) return true;
        }
    }
#endif

    std::string cachedPath;
    for (const auto& path : relativePaths) {
        if (isCached(path)) {
            cachedPath = path;
            break;
        }
    }

    if (!cachedPath.empty()) {
        return apply(image, cachedPath);
    }

    for (const auto& path : relativePaths) {
        if (apply(image, path)) return true;
    }
    return false;
}

void IconTextureCache::warmPath(const std::string& relativePath) {
    if (relativePath.empty()) return;

#ifdef TOTK_ICON_ATLAS
    if (IconAtlas::instance().ensureLoaded() && IconAtlas::instance().hasEntry(relativePath)) {
        return;
    }
#endif

    const std::string key = cacheKey(relativePath);
    if (brls::TextureCache::instance().hasCacheKey(key)) return;

    brls::Image image;
    image.setImageFromRes(relativePath);
    if (image.getOriginalImageWidth() <= 0.0f || image.getOriginalImageHeight() <= 0.0f) {
        return;
    }

    textureCount_++;
    const std::string path = "romfs:/" + relativePath;
    FILE* file = std::fopen(path.c_str(), "rb");
    if (file) {
        std::fseek(file, 0, SEEK_END);
        const long size = std::ftell(file);
        std::fclose(file);
        if (size > 0) {
            bytesCached_ += static_cast<size_t>(size);
        }
    }
}

void IconTextureCache::warmPaths(const std::vector<std::string>& relativePaths) {
    for (const auto& path : relativePaths) {
        warmPath(path);
    }
}

namespace {

void warmPathsBatchedChunk(const std::shared_ptr<std::vector<std::string>>& paths, size_t startIndex,
                           size_t entriesBefore, size_t bytesBefore, std::function<void()> onComplete) {
    constexpr size_t kBatchSize = 6;
    const size_t end = std::min(startIndex + kBatchSize, paths->size());
    const uint64_t batchStartMs = perfNowMs();

    for (size_t i = startIndex; i < end; ++i) {
        IconTextureCache::instance().warmPath((*paths)[i]);
    }

    if (end < paths->size()) {
        TOTK_PERF("icon_cache.warm_batch progress=%zu/%zu ms=%llu", end, paths->size(),
                  static_cast<unsigned long long>(perfNowMs() - batchStartMs));
        brls::sync([paths, end, entriesBefore, bytesBefore, onComplete = std::move(onComplete)]() {
            warmPathsBatchedChunk(paths, end, entriesBefore, bytesBefore, std::move(onComplete));
        });
        return;
    }

    auto& cache = IconTextureCache::instance();
    TOTK_PERF("icon_cache warmed entries=%zu bytes=%zu added=%zu batch_bytes=%zu paths=%zu",
              cache.entryCount(), cache.bytesCached(), cache.entryCount() - entriesBefore,
              cache.bytesCached() - bytesBefore, paths->size());
    if (onComplete) {
        onComplete();
    }
}

}  // namespace

void IconTextureCache::warmPathsBatched(std::vector<std::string> relativePaths,
                                        std::function<void()> onComplete) {
    if (relativePaths.empty()) {
        TOTK_PERF("icon_cache warmed entries=%zu bytes=%zu added=0 batch_bytes=0 paths=0", entryCount(),
                  bytesCached());
        if (onComplete) {
            onComplete();
        }
        return;
    }

    TOTK_LOG("icon_cache warm begin paths=%zu", relativePaths.size());
    const size_t entriesBefore = entryCount();
    const size_t bytesBefore = bytesCached();
    auto paths = std::make_shared<std::vector<std::string>>(std::move(relativePaths));
    warmPathsBatchedChunk(paths, 0, entriesBefore, bytesBefore, std::move(onComplete));
}

size_t IconTextureCache::entryCount() const { return textureCount_; }

size_t IconTextureCache::bytesCached() const { return bytesCached_; }

void IconTextureCache::clear() {
    textureCount_ = 0;
    bytesCached_ = 0;
}

}  // namespace totk
