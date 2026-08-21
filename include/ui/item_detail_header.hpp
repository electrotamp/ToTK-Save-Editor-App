#pragma once

#include <borealis.hpp>

#include "ui/pouch_item_display.hpp"

namespace totk::ui {

class ItemFocusHeader : public brls::Box {
public:
    ItemFocusHeader();

    void configure(const totk::ItemDisplayInfo& info, float iconSize = 72.0f);
    void applyThemeColors();

private:
    brls::Image* baseIcon_ = nullptr;
    brls::Label* name_ = nullptr;
    brls::Label* subtitle_ = nullptr;
    brls::Box* fusePanel_ = nullptr;
    brls::Label* fuseTitle_ = nullptr;
    brls::Image* fuseIcon_ = nullptr;
    brls::Label* fuseNone_ = nullptr;
};

brls::Box* createItemDetailHeader(const totk::ItemDisplayInfo& info, float iconSize = 72.0f);

}  // namespace totk::ui
