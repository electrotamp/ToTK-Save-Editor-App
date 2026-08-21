#pragma once

#include <borealis.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace totk {

bool loadRomfsImage(brls::Image* image, const std::string& relativePath);
bool loadRomfsImageFirst(brls::Image* image, const std::vector<std::string>& relativePaths);

// Returns false if bytes are missing, too large, not JPEG, or GPU decode fails.
bool loadJpegFromMem(brls::Image* image, const std::vector<uint8_t>& jpegBytes);

// Returns false if bytes are missing, too large, not PNG, or GPU decode fails.
bool loadPngFromMem(brls::Image* image, const std::vector<uint8_t>& pngBytes);

}  // namespace totk
