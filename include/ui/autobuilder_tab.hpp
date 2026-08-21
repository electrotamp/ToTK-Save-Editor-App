#pragma once

#include <borealis.hpp>

namespace totk::ui {

class BoundedNumericCell;

class AutobuilderTab : public brls::ScrollingFrame {
public:
    AutobuilderTab();
    static brls::View* create();

private:
    int selectedSlot_ = 0;
    BoundedNumericCell* indexCell_ = nullptr;
};

}  // namespace totk::ui
