#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace brls {
class Image;
}

namespace totk {

struct IconAtlasRect {
    std::string atlasPath;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

class IconAtlas {
public:
    static IconAtlas& instance();

    bool loadFromRomfs();
    bool ensureLoaded();
    bool enabled() const { return loaded_; }

    std::optional<IconAtlasRect> lookup(const std::string& relativePath) const;
    bool hasEntry(const std::string& relativePath) const;

    void preloadAtlases();
    bool apply(brls::Image* image, const std::string& relativePath);
    bool ensureSheetResident(const std::string& atlasRelativePath);

    size_t entryCount() const { return entries_.size(); }
    size_t atlasCount() const { return atlasPaths_.size(); }
    size_t residentSheetCount() const { return residentSheets_.size(); }

private:
    static constexpr size_t kMaxResidentSheets = 2;

    bool loaded_ = false;
    std::unordered_map<std::string, IconAtlasRect> entries_;
    std::vector<std::string> atlasPaths_;

    struct ResidentSheet {
        std::string path;
        int texture = 0;
        uint64_t lastUseMs = 0;
    };
    std::vector<ResidentSheet> residentSheets_;

    void touchSheet(const std::string& atlasRelativePath);
    void evictLeastRecentSheet();
};

}  // namespace totk
