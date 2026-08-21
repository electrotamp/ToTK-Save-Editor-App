#pragma once

#include <map>
#include <string>
#include <vector>

namespace totk {

class CompletismData {
public:
    static CompletismData& instance();
    bool loadFromRomfs();

    const std::vector<uint32_t>& hashesFor(const std::string& key) const;
    const std::vector<std::vector<float>>& coordinatesFor(const std::string& key) const;

private:
    CompletismData() = default;
    std::map<std::string, std::vector<uint32_t>> hashGroups_;
    std::map<std::string, std::vector<std::vector<float>>> coordinateGroups_;
};

}  // namespace totk
