#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace totk::save_editor {

// Canonical stamina-wheel table (raw game float-bit-pattern values -> display
// labels) — shared between the Stats tab's selector (TabHostActivity) and the
// picker's detail panel segment bar (PickerActivity), kept in one place here
// so both stay in sync. The table itself lives in save_editor_stats.cpp.
const std::vector<std::string>& staminaLabels();
uint32_t staminaOptionValue(int index);
int staminaSelectionForValue(uint32_t value);

// Index of the closest entry in the table for a raw maxStamina value, and the
// table's size — exposed so the picker's detail panel can render a
// normalized segment bar (index / (count - 1)) without duplicating the
// table, which stores raw game float-bit-pattern values, not a simple linear
// scale.
int staminaWheelSegmentIndex(uint32_t value);
int staminaWheelSegmentCount();

}  // namespace totk::save_editor
