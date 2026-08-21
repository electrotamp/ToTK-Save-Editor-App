#include "ui/autobuilder_tab.hpp"

#include "app/app_state.hpp"
#include "ui/bounded_numeric_cell.hpp"

#include <vector>

namespace totk::ui {

AutobuilderTab::AutobuilderTab() {
    this->setWidthPercentage(100);
    this->setHeightPercentage(100);

    auto* content = new brls::Box();
    content->setAxis(brls::Axis::COLUMN);
    content->setWidthPercentage(100);

    auto* header = new brls::Header();
    header->setTitle("Autobuilder Favorites");
    content->addView(header);

    auto* hint = new brls::Label();
    hint->setText("Tap a row to edit, just like the Home tab. Press Y to save.");
    hint->setFontSize(16);
    hint->setTextColor(nvgRGB(160, 160, 160));
    hint->setMarginBottom(8);
    content->addView(hint);

    auto& slots = AppState::instance().editor().autobuilderSlots();
    if (slots.empty()) {
        auto* empty = new brls::Label();
        empty->setText("No autobuilder data loaded.");
        content->addView(empty);
        this->setContentView(content);
        return;
    }

    std::vector<std::string> labels;
    labels.reserve(slots.size());
    for (size_t i = 0; i < slots.size(); ++i) {
        labels.push_back("Slot " + std::to_string(i + 1));
    }

    selectedSlot_ = 0;
    auto* selector = new brls::SelectorCell();
    selector->init(
        "Blueprint Slot", labels, selectedSlot_,
        [this, &slots](int selected) {
            selectedSlot_ = selected;
            if (indexCell_) {
                indexCell_->setValue(slots[static_cast<size_t>(selectedSlot_)].slotValue, true);
            }
        });
    content->addView(selector);

    indexCell_ = new BoundedNumericCell();
    indexCell_->init(
        "Favorite Index", slots[0].slotValue, -1, 29,
        [](long value) { return std::clamp(value, -1L, 29L); },
        [&slots, this](long value) {
            auto& slot = slots[static_cast<size_t>(selectedSlot_)];
            slot.slotValue = static_cast<int>(value);
            slot.favorite = slot.slotValue >= 0;
        },
        "-1 to 29", true);
    content->addView(indexCell_);

    this->setContentView(content);
}

brls::View* AutobuilderTab::create() { return new AutobuilderTab(); }

}  // namespace totk::ui
