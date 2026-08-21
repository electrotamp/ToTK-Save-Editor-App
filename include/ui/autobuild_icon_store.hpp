#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace totk::ui {

// HyruleWorks preview images are keyed by a deterministic fingerprint of
// the 6688-byte CombinedActorInfo blob, not by the draft array position.
// TotK reuses physical draft positions as its 30-entry history ring turns;
// a position-keyed image therefore becomes stale as soon as the game writes
// a different build into that position. Content identity lets an icon follow
// its blueprint and automatically stops matching after a position is reused.
std::string autobuildIconKey(const std::vector<uint8_t>& combinedActorInfo);
std::string autobuildIconPath(const std::string& blueprintKey);

// Persists `imageBytes` (already downloaded, JPEG) for this exact blueprint.
bool saveAutobuildIcon(const std::vector<uint8_t>& combinedActorInfo, const std::vector<uint8_t>& imageBytes);

// Loads a previously-saved icon for this exact blueprint. Returns empty if
// the build was not imported from a source that supplied a preview image.
std::vector<uint8_t> loadAutobuildIcon(const std::vector<uint8_t>& combinedActorInfo);

}  // namespace totk::ui
