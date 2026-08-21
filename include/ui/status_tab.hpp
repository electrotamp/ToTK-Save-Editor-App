#pragma once

#include <cstdint>

#include <borealis.hpp>

namespace totk::ui {

class BoundedNumericCell;

class StatusTab : public brls::ScrollingFrame {
public:
    StatusTab();
    static brls::View* create();
    void refresh();
    void apply();
    void applyThemeColors();
    void activatePageFocus();
    brls::View* primaryFocusCell() const { return defaultFocusCell_; }

    brls::View* getDefaultFocus() override;
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;
    void onChildFocusGained(brls::View* directChild, brls::View* focusedView) override;
    void willAppear(bool resetState = false) override;

private:
    int heartSelectionForValue(uint32_t quarters) const;
    int staminaSelectionForValue(uint32_t value) const;
    int batterySelectionForValue(float value) const;

    brls::Box* content = nullptr;
    brls::Label* versionLabel = nullptr;
    brls::Label* playtimeLabel = nullptr;
    BoundedNumericCell* rupeesCell = nullptr;
    brls::View* defaultFocusCell_ = nullptr;
    brls::SelectorCell* heartsCell = nullptr;
    brls::SelectorCell* staminaCell = nullptr;
    brls::SelectorCell* batteryCell = nullptr;
    BoundedNumericCell* ponyCell = nullptr;
    BoundedNumericCell* pouchWeaponsCell = nullptr;
    BoundedNumericCell* pouchBowsCell = nullptr;
    BoundedNumericCell* pouchShieldsCell = nullptr;
    BoundedNumericCell* posXCell = nullptr;
    BoundedNumericCell* posYCell = nullptr;
    BoundedNumericCell* posZCell = nullptr;
    brls::Label* mapPinsLabel = nullptr;
    brls::Label* themeLabel = nullptr;
};

}  // namespace totk::ui
