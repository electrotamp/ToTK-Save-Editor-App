#include "save/location_names.hpp"

#include <nlohmann/json.hpp>

#include "util/resource_loader.hpp"

namespace totk {

LocationNames& LocationNames::instance() {
    static LocationNames data;
    return data;
}

LocationNames::LocationNames() { load(); }

void LocationNames::load() {
    std::unordered_map<std::string, std::string> loaded;
    try {
        const auto json = nlohmann::json::parse(totk::loadResourceText("data/location_names.json"));
        for (auto it = json.begin(); it != json.end(); ++it) {
            // Skip non-string entries (e.g. a stray metadata object) instead
            // of letting one bad key throw away everything already parsed.
            if (!it.value().is_string()) continue;
            loaded[it.key()] = it.value().get<std::string>();
        }
    } catch (...) {
        return;
    }
    names_ = std::move(loaded);
}

std::string LocationNames::lookup(const std::string& id) const {
    if (id.empty()) return {};
    const auto it = names_.find(id);
    return it == names_.end() ? std::string{} : it->second;
}

}  // namespace totk
