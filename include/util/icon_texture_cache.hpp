#pragma once

#include <borealis.hpp>
#include <functional>
#include <string>
#include <vector>

namespace totk {

class IconTextureCache {
public:
    static IconTextureCache& instance();

    bool apply(brls::Image* image, const std::string& relativePath);
    bool applyFirst(brls::Image* image, const std::vector<std::string>& relativePaths);

    bool hasAsset(const std::string& relativePath) const;
    bool isCached(const std::string& relativePath) const;

    void warmPath(const std::string& relativePath);
    void warmPaths(const std::vector<std::string>& relativePaths);
    void warmPathsBatched(std::vector<std::string> relativePaths, std::function<void()> onComplete);

    size_t entryCount() const;
    size_t bytesCached() const;
    void clear();

    static void ensureCapacity(size_t expectedTextures);

private:
    static std::string cacheKey(const std::string& relativePath);

    size_t textureCount_ = 0;
    size_t bytesCached_ = 0;
};

}  // namespace totk
