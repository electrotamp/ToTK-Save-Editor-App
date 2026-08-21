#include "save/completism_data.hpp"

#include <nlohmann/json.hpp>

#include "util/resource_loader.hpp"

namespace totk {

CompletismData& CompletismData::instance() {
    static CompletismData data;
    return data;
}

bool CompletismData::loadFromRomfs() {
    try {
        const auto hashJson = nlohmann::json::parse(totk::loadResourceText("data/completism.json"));
        const auto coordJson = nlohmann::json::parse(totk::loadResourceText("data/coordinates.json"));
        hashGroups_.clear();
        coordinateGroups_.clear();
        for (auto it = hashJson.begin(); it != hashJson.end(); ++it) {
            std::vector<uint32_t> values;
            for (const auto& value : it.value()) values.push_back(value.get<uint32_t>());
            hashGroups_[it.key()] = std::move(values);
        }
        for (auto it = coordJson.begin(); it != coordJson.end(); ++it) {
            std::vector<std::vector<float>> values;
            for (const auto& triple : it.value()) {
                values.push_back({triple[0].get<float>(), triple[1].get<float>(), triple[2].get<float>()});
            }
            coordinateGroups_[it.key()] = std::move(values);
        }
        return true;
    } catch (...) {
        return false;
    }
}

const std::vector<uint32_t>& CompletismData::hashesFor(const std::string& key) const {
    static const std::vector<uint32_t> empty;
    const auto it = hashGroups_.find(key);
    return it == hashGroups_.end() ? empty : it->second;
}

const std::vector<std::vector<float>>& CompletismData::coordinatesFor(const std::string& key) const {
    static const std::vector<std::vector<float>> empty;
    const auto it = coordinateGroups_.find(key);
    return it == coordinateGroups_.end() ? empty : it->second;
}

}  // namespace totk
