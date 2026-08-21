#include "ui/completion_tab.hpp"

#include "app/app_state.hpp"

namespace totk::ui {

CompletionTab::CompletionTab() {
    this->setWidthPercentage(100);
    this->setHeightPercentage(100);

    auto* content = new brls::Box();
    content->setAxis(brls::Axis::COLUMN);
    content->setWidthPercentage(100);

    auto* header = new brls::Header();
    header->setTitle("Completionism");
    content->addView(header);

    auto* hint = new brls::Label();
    hint->setText("Unlock all entries or pin missing locations on the map. Press Y to save.");
    hint->setMarginBottom(12);
    content->addView(hint);

    addCategory(content, "Towers", "towers_activated", "towers");
    addCategory(content, "Shrines", "shrines_status", "shrines");
    addCategory(content, "Lightroots", "lightroots_status", "lightroots");
    addCategory(content, "Koroks (hidden)", "koroks_hidden", "koroks_hidden");
    addCategory(content, "Koroks (carry)", "koroks_carry", "koroks_carry");
    addCategory(content, "Locations", "locations_visited", "locations");
    addCategory(content, "Caves", "location_caves_visited", "location_caves");
    addCategory(content, "Wells", "location_wells_visited", "location_wells");
    addCategory(content, "Bubbuls", "bubbuls_defeated", "location_bubbuls");
    addCategory(content, "Flux Constructs", "bosses_flux_construct_defeated", "bosses_flux_construct");
    addCategory(content, "Hinoxes", "bosses_hinoxes_defeated", "bosses_hinoxes");
    addCategory(content, "Taluses", "bosses_taluses_defeated", "bosses_taluses");
    addCategory(content, "Moldugas", "bosses_moldugas_defeated", "bosses_moldugas");
    addCategory(content, "Froxs", "bosses_froxs_defeated", "bosses_froxs");
    addCategory(content, "Gleeoks", "bosses_gleeoks_defeated", "bosses_gleeoks");
    addCategory(content, "Boss Rematches", "bosses_rematch_defeated", "bosses_rematch");
    addCategory(content, "Treasure Maps", "treasure_maps_found", "treasure_maps");
    addCategory(content, "Addison Signs", "addison_completed", "");

    this->setContentView(content);
}

void CompletionTab::addCategory(brls::Box* content, const std::string& label, const std::string& unlockKey,
                                const std::string& pinCategory) {
    auto* section = new brls::Header();
    section->setTitle(label);
    section->setMarginTop(8);
    content->addView(section);

    auto* unlockCell = new brls::DetailCell();
    unlockCell->setText("Unlock All");
    unlockCell->setDetailText("Mark complete");
    unlockCell->registerClickAction([unlockKey](brls::View*) {
        if (AppState::instance().editor().unlockAllCompletion(unlockKey)) {
            brls::Application::notify("Unlocked " + unlockKey);
        } else {
            brls::Application::notify("Nothing to unlock");
        }
        return true;
    });
    content->addView(unlockCell);

    if (!pinCategory.empty()) {
        auto* pinCell = new brls::DetailCell();
        pinCell->setText("Pin Missing");
        pinCell->setDetailText("Up to 50");
        pinCell->registerClickAction([pinCategory](brls::View*) {
            if (AppState::instance().editor().pinMissingLocations(pinCategory, 50)) {
                brls::Application::notify("Pinned missing " + pinCategory);
            } else {
                brls::Application::notify("No free pins or nothing missing");
            }
            return true;
        });
        pinCell->setMarginBottom(4);
        content->addView(pinCell);
    }
}

brls::View* CompletionTab::create() { return new CompletionTab(); }

}  // namespace totk::ui
