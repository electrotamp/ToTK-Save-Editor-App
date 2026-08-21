#pragma once

#include <string>
#include <vector>

namespace totk {

struct ItemDisplayInfo {
    std::string id;
    std::string name;
    std::string badge;
    std::string subtitle;
    std::string fuseId;
    std::string fuseName;
    std::string tackName;
    bool showFusePanel = false;
    bool showTackPanel = false;
    std::vector<std::string> iconPaths;
    std::vector<std::string> fuseIconPaths;
    std::vector<std::string> tackIconPaths;
};

class SaveEditor;

ItemDisplayInfo displayInfoForPouchItem(SaveEditor& editor, const std::string& category, int index);

}  // namespace totk
