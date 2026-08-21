#include "save_editor/save_editor_stats.hpp"

#include "save/game_limits.hpp"

#include <cstddef>

namespace totk::save_editor {

namespace {

struct StaminaOption {
    uint32_t value;
    const char* label;
};

// Same fixed table as the pre-atlas reference's Stats tab (source/ui/status_tab.cpp).
constexpr StaminaOption kStaminaOptions[] = {
    {1148846080u, "1 wheel"},
    {1150681088u, "1 wheel + 1/5"},
    {1152319488u, "1 wheel + 2/5"},
    {1153957888u, "1 wheel + 3/5"},
    {1155596288u, "1 wheel + 4/5"},
    {1157234688u, "2 wheels"},
    {1158250496u, "2 wheels + 1/5"},
    {1159069696u, "2 wheels + 2/5"},
    {1159888896u, "2 wheels + 3/5"},
    {1160708096u, "2 wheels + 4/5"},
    {1161527296u, "3 wheels"},
    {1342177279u, "Infinite (editor)"},
};

}  // namespace

const std::vector<std::string>& staminaLabels() {
    static const std::vector<std::string> labels = [] {
        std::vector<std::string> result;
        for (const auto& option : kStaminaOptions) result.emplace_back(option.label);
        return result;
    }();
    return labels;
}

uint32_t staminaOptionValue(int index) {
    if (index < 0 || static_cast<size_t>(index) >= std::size(kStaminaOptions)) return 0;
    return kStaminaOptions[static_cast<size_t>(index)].value;
}

int staminaSelectionForValue(uint32_t value) {
    const uint32_t clamped = totk::GameLimits::clampMaxStamina(static_cast<long>(value));
    for (size_t i = 0; i < std::size(kStaminaOptions); ++i) {
        if (kStaminaOptions[i].value == clamped) return static_cast<int>(i);
    }
    return static_cast<int>(std::size(kStaminaOptions)) - 2;
}

int staminaWheelSegmentIndex(uint32_t value) { return staminaSelectionForValue(value); }
int staminaWheelSegmentCount() { return static_cast<int>(std::size(kStaminaOptions)); }

}  // namespace totk::save_editor
