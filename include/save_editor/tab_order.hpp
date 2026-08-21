#pragma once

#include <string>

namespace totk::save_editor {

// Canonical tab order for L/R navigation across the editor. Mirrors the old
// build's tab strip order (Stats, Weapons, Bows, Shields, Armors, Materials,
// Food, Zonai Devices, Key Items, Horses), with one deliberate change: Shields
// was moved to sit right after Stats, ahead of Weapons, per explicit request.
std::string tabDisplayName(const std::string& tabId);

// Fills prevId/nextId with this tab's L/R neighbors in the canonical order,
// leaving either empty if there is none (Stats has no prev, Horses has no
// next). Used by TabHostActivity to wire L/R directly to switchToTab() —
// there is no separate per-tab Activity to pop/push anymore.
void tabNeighbors(const std::string& tabId, std::string& prevId, std::string& nextId);

}  // namespace totk::save_editor
