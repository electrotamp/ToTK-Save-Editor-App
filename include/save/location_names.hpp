#pragma once

#include <string>
#include <unordered_map>

namespace totk {

// Looks up the real in-game display name for an internal TotK location id
// (e.g. "Cave_CentralHyrule_0018" -> "Royal Hidden Passage",
// "MapArea_CentralHyrule" -> "Hyrule Field"). Table sourced from the game's
// own StaticMsg/LocationMarker + StaticMsg/Dungeon message archives (via
// objmap-totk.zeldamods.org's extracted game_files), not derivable from save
// data alone. Returns an empty string when the id isn't in the table.
class LocationNames {
public:
    static LocationNames& instance();

    std::string lookup(const std::string& id) const;

private:
    LocationNames();
    void load();

    std::unordered_map<std::string, std::string> names_;
};

}  // namespace totk
