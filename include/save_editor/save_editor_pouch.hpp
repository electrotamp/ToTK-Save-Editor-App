#pragma once

#include <borealis.hpp>
#include <cstddef>
#include <string>

namespace totk::save_editor {

// Per-item edit page for the pouch category families (armors, the four stack
// categories materials/food/devices/key, and horses) — dispatches to the
// matching totk::ui::ItemEditorUi::addXEditor. The grid these are opened
// from is TabHostActivity::buildPouchGrid(), not a dedicated Activity — see
// tab_host_activity.hpp.
class PouchEditActivity : public brls::Activity {
public:
    PouchEditActivity(std::string category, size_t index);

    brls::View* createContentView() override;

private:
    std::string category_;
    size_t index_;

    brls::Image* headerIcon_ = nullptr;
    brls::Label* headerName_ = nullptr;
    brls::Label* headerSubtitle_ = nullptr;

    void refreshHeader();
    void confirmDelete();
};

}  // namespace totk::save_editor
