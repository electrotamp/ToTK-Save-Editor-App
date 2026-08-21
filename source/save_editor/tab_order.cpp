#include "save_editor/tab_order.hpp"

#include <algorithm>
#include <vector>

namespace totk::save_editor {

namespace {

const std::vector<std::string>& tabOrder() {
    static const std::vector<std::string> order = {"stats",     "shields", "weapons",   "bows",  "armors",
                                                    "materials", "food",    "devices",   "key",   "horses",
                                                    "autobuild", "credits"};
    return order;
}

}  // namespace

std::string tabDisplayName(const std::string& tabId) {
    if (tabId == "stats") return "Stats";
    if (tabId == "shields") return "Shields";
    if (tabId == "weapons") return "Weapons";
    if (tabId == "bows") return "Bows";
    if (tabId == "armors") return "Armors";
    if (tabId == "materials") return "Materials";
    if (tabId == "food") return "Food";
    if (tabId == "devices") return "Zonai Devices";
    if (tabId == "key") return "Key Items";
    if (tabId == "horses") return "Horses";
    if (tabId == "autobuild") return "Autobuild";
    if (tabId == "credits") return "About";
    return tabId;
}

void tabNeighbors(const std::string& tabId, std::string& prevId, std::string& nextId) {
    prevId.clear();
    nextId.clear();
    const auto& order = tabOrder();
    if (order.empty()) return;
    const auto it = std::find(order.begin(), order.end(), tabId);
    if (it == order.end()) return;
    const size_t index = static_cast<size_t>(it - order.begin());
    const size_t count = order.size();
    // Wraps at both ends so L/R cycle through every tab endlessly instead of
    // stopping dead at Stats/About.
    prevId = order[(index + count - 1) % count];
    nextId = order[(index + 1) % count];
}

}  // namespace totk::save_editor
