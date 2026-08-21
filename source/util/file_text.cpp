#include "util/file_text.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#if defined(__SWITCH__)
#include <dirent.h>
#include <sys/stat.h>
#include <switch.h>
#endif

namespace totk {

namespace {

std::string parentPath(const std::string& path) {
    const size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) return {};
    return path.substr(0, slash);
}

bool endsWithIgnoreCase(const std::string& value, const std::string& suffix) {
    if (value.size() < suffix.size()) return false;
    for (size_t i = 0; i < suffix.size(); ++i) {
        const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(value[value.size() - suffix.size() + i])));
        const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(suffix[i])));
        if (a != b) return false;
    }
    return true;
}

}  // namespace

std::string loadFileText(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return {};
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (size <= 0) {
        std::fclose(file);
        return {};
    }
    std::string content(static_cast<size_t>(size), '\0');
    const size_t read = std::fread(content.data(), 1, content.size(), file);
    std::fclose(file);
    content.resize(read);
    return content;
}

bool writeFileText(const std::string& path, const std::string& content) {
    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) return false;
    const size_t written = std::fwrite(content.data(), 1, content.size(), file);
    std::fclose(file);
    return written == content.size();
}

bool ensureParentDirectory(const std::string& filePath) {
#if defined(__SWITCH__)
    const std::string parent = parentPath(filePath);
    if (parent.empty()) return true;
    std::string built;
    for (size_t i = 0; i < parent.size(); ++i) {
        if (parent[i] == '/' && !built.empty()) {
            mkdir(built.c_str(), 0777);
        }
        built.push_back(parent[i]);
    }
    if (!built.empty()) mkdir(built.c_str(), 0777);
    return true;
#else
    (void)filePath;
    return true;
#endif
}

std::vector<FileBrowserEntry> listDirectoryEntries(const std::string& directoryPath, const std::string& extensionFilter) {
    std::vector<FileBrowserEntry> entries;

#if defined(__SWITCH__)
    DIR* dir = opendir(directoryPath.c_str());
    if (!dir) return entries;

    if (directoryPath != "sdmc:/") {
        entries.push_back(FileBrowserEntry{"..", parentPath(directoryPath), true});
    }

    while (dirent* entry = readdir(dir)) {
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) continue;

        const bool needsSlash = directoryPath.empty() || directoryPath.back() != '/';
        const std::string fullPath = directoryPath + (needsSlash ? "/" : "") + entry->d_name;
        struct stat st {};
        if (stat(fullPath.c_str(), &st) != 0) continue;

        const bool isDirectory = S_ISDIR(st.st_mode);
        if (!isDirectory && !extensionFilter.empty() && !endsWithIgnoreCase(entry->d_name, extensionFilter)) continue;

        entries.push_back(FileBrowserEntry{entry->d_name, fullPath, isDirectory});
    }

    closedir(dir);

    std::sort(entries.begin() + (entries.empty() || entries[0].name != ".." ? 0 : 1), entries.end(),
              [](const FileBrowserEntry& a, const FileBrowserEntry& b) {
                  if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;
                  return a.name < b.name;
              });
#else
    (void)extensionFilter;
    const std::string localDir = "wisp profiles";
    entries.push_back(FileBrowserEntry{"..", ".", true});
    std::ifstream probe;
    (void)probe;
    entries.push_back(FileBrowserEntry{"resources/data/zonai_wisps.json", "resources/data/zonai_wisps.json", false});
    (void)localDir;
    (void)directoryPath;
#endif

    return entries;
}

}  // namespace totk
