#pragma once

#include <borealis.hpp>
#include <cstddef>
#include <string>

namespace totk::save_editor {

// Full per-item equipment edit page (durability / modifier / fuse / delete),
// matching the reference editor's field-row layout. Category-generic — used
// for weapons, shields, or anything else sharing the same EquipmentItem shape
// (see TabHostActivity::buildEquipmentGrid in tab_host_activity.cpp). Reuses totk::ui::ItemEditorUi
// for the field widgets and totk::displayInfoForPouchItem for display text —
// both are standalone data/UI helpers already shared with the legacy fork,
// not part of its focus-restore/grid-caching machinery. The header icon is
// loaded via IconAtlas (not the legacy per-PNG loader), matching the
// editor's asset path.
class EquipmentEditActivity : public brls::Activity {
public:
    EquipmentEditActivity(std::string category, size_t index);

    brls::View* createContentView() override;

private:
    std::string category_;
    size_t index_;

    brls::Image* headerIcon_ = nullptr;
    brls::Label* headerName_ = nullptr;
    brls::Label* headerSubtitle_ = nullptr;
    brls::Image* headerFuseIcon_ = nullptr;
    brls::Label* headerFuseLabel_ = nullptr;
    brls::DetailCell* fuseCell_ = nullptr;

    void refreshHeader();
    void confirmDelete();
    void openFusePicker();
};

}  // namespace totk::save_editor
