#include "util/resource_loader.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

namespace totk {

std::string loadResourceText(const std::string& relativePath) {
#if defined(__SWITCH__)
    const std::string path = "romfs:/" + relativePath;
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
#else
    const std::string diskPath = "resources/" + relativePath;
    std::ifstream file(diskPath, std::ios::binary);
    if (!file) return {};
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
#endif
}

}  // namespace totk
