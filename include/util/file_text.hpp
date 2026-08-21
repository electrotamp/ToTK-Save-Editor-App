#pragma once

#include <string>
#include <vector>

namespace totk {

std::string loadFileText(const std::string& path);
bool writeFileText(const std::string& path, const std::string& content);
bool ensureParentDirectory(const std::string& filePath);

struct FileBrowserEntry {
    std::string name;
    std::string fullPath;
    bool isDirectory = false;
};

// extensionFilter: when non-empty, only files ending in it (case-insensitive,
// e.g. ".json" or ".cai") are listed; directories always pass through.
std::vector<FileBrowserEntry> listDirectoryEntries(const std::string& directoryPath, const std::string& extensionFilter);

}  // namespace totk
