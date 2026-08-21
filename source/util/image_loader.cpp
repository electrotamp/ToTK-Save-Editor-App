#include "util/image_loader.hpp"

#include <cstring>
#include <vector>

#include "util/icon_texture_cache.hpp"

namespace totk {

namespace {

constexpr size_t kMaxJpegBytes = 512u * 1024u;
constexpr size_t kMaxPngBytes = 512u * 1024u;

bool looksLikeJpeg(const std::vector<uint8_t>& bytes) {
    return bytes.size() >= 3 && bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF;
}

bool looksLikePng(const std::vector<uint8_t>& bytes) {
    static const uint8_t kSignature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    return bytes.size() >= 8 && std::memcmp(bytes.data(), kSignature, sizeof(kSignature)) == 0;
}

}  // namespace

bool loadJpegFromMem(brls::Image* image, const std::vector<uint8_t>& jpegBytes) {
    if (!image || jpegBytes.empty() || jpegBytes.size() > kMaxJpegBytes || !looksLikeJpeg(jpegBytes)) {
        return false;
    }

    image->setImageFromMem(jpegBytes.data(), static_cast<int>(jpegBytes.size()));
    return image->getOriginalImageWidth() > 0.0f && image->getOriginalImageHeight() > 0.0f;
}

bool loadPngFromMem(brls::Image* image, const std::vector<uint8_t>& pngBytes) {
    if (!image || pngBytes.empty() || pngBytes.size() > kMaxPngBytes || !looksLikePng(pngBytes)) {
        return false;
    }

    image->setImageFromMem(pngBytes.data(), static_cast<int>(pngBytes.size()));
    return image->getOriginalImageWidth() > 0.0f && image->getOriginalImageHeight() > 0.0f;
}

bool loadRomfsImage(brls::Image* image, const std::string& relativePath) {
    return IconTextureCache::instance().apply(image, relativePath);
}

bool loadRomfsImageFirst(brls::Image* image, const std::vector<std::string>& relativePaths) {
    return IconTextureCache::instance().applyFirst(image, relativePaths);
}

}  // namespace totk
