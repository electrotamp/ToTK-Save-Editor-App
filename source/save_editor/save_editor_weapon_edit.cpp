#include "save_editor/save_editor_weapon_edit.hpp"

#include "app/app_state.hpp"
#include "save_editor/save_editor_item_picker.hpp"
#include "save/game_limits.hpp"
#include "save/save_editor.hpp"
#include "ui/item_database.hpp"
#include "ui/item_editor_ui.hpp"
#include "ui/pouch_item_display.hpp"
#include "ui/zonai_wisp_overlay.hpp"
#include "util/icon_atlas.hpp"
#include "util/totk_log.hpp"

#include <borealis/core/application.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/core/touch/tap_gesture.hpp>

namespace totk::save_editor {

EquipmentEditActivity::EquipmentEditActivity(std::string category, size_t index)
    : category_(std::move(category)), index_(index) {}

brls::View* EquipmentEditActivity::createContentView() {
    auto& editor = AppState::instance().editor();
    const auto info = totk::displayInfoForPouchItem(editor, category_, static_cast<int>(index_));

    auto* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1.0f);

    auto* content = new brls::Box();
    content->setAxis(brls::Axis::COLUMN);
    content->setWidthPercentage(100);
    content->setPadding(24, 24, 24, 24);

    auto* hint = new brls::Label();
    hint->setText("Edit values below. Press B to return to the item grid.");
    hint->setFontSize(15);
    hint->setTextColor(nvgRGB(160, 160, 160));
    hint->setMarginBottom(10);
    content->addView(hint);

    auto* headerRow = new brls::Box();
    headerRow->setAxis(brls::Axis::ROW);
    headerRow->setAlignItems(brls::AlignItems::CENTER);
    headerRow->setWidthPercentage(100);
    headerRow->setMarginBottom(16);
    headerRow->setFocusable(false);

    headerIcon_ = new brls::Image();
    headerIcon_->setScalingType(brls::ImageScalingType::FIT);
    headerIcon_->setWidth(72);
    headerIcon_->setHeight(72);
    headerIcon_->setFocusable(false);
    headerIcon_->setMarginRight(12);
    headerRow->addView(headerIcon_);

    auto* textColumn = new brls::Box();
    textColumn->setAxis(brls::Axis::COLUMN);
    textColumn->setGrow(1.0f);
    textColumn->setFocusable(false);

    headerName_ = new brls::Label();
    headerName_->setFontSize(22);
    headerName_->setFocusable(false);
    textColumn->addView(headerName_);

    headerSubtitle_ = new brls::Label();
    headerSubtitle_->setFontSize(16);
    headerSubtitle_->setTextColor(nvgRGB(160, 160, 160));
    headerSubtitle_->setFocusable(false);
    textColumn->addView(headerSubtitle_);

    headerRow->addView(textColumn);

    // Bows use arrow attachments, not the weapon/shield Fuse save field.
    if (category_ != "bows") {
        auto* fuseColumn = new brls::Box();
        fuseColumn->setAxis(brls::Axis::COLUMN);
        fuseColumn->setAlignItems(brls::AlignItems::CENTER);
        fuseColumn->setFocusable(false);
        fuseColumn->setMarginLeft(12);

        auto* fuseTitle = new brls::Label();
        fuseTitle->setText("Fuse");
        fuseTitle->setFontSize(13);
        fuseTitle->setTextColor(nvgRGB(160, 160, 160));
        fuseTitle->setFocusable(false);
        fuseTitle->setMarginBottom(4);
        fuseColumn->addView(fuseTitle);

        headerFuseIcon_ = new brls::Image();
        headerFuseIcon_->setScalingType(brls::ImageScalingType::FIT);
        headerFuseIcon_->setWidth(48);
        headerFuseIcon_->setHeight(48);
        headerFuseIcon_->setFocusable(false);
        fuseColumn->addView(headerFuseIcon_);

        headerFuseLabel_ = new brls::Label();
        headerFuseLabel_->setFontSize(15);
        headerFuseLabel_->setFocusable(false);
        fuseColumn->addView(headerFuseLabel_);

        headerRow->addView(fuseColumn);
    }
    content->addView(headerRow);

    auto& items = editor.equipment()[category_];
    if (index_ < items.size()) {
        auto& item = items[index_];
        const bool withFuse = category_ != "bows";
        fuseCell_ = totk::ui::ItemEditorUi::addEquipmentEditor(
            content, item, withFuse, /*withModifier=*/true,
            withFuse ? [this]() { openFusePicker(); } : std::function<void()>());
    }

    auto* deleteCell = new brls::DetailCell();
    deleteCell->setText("Delete Item");
    deleteCell->setDetailText("Remove from pouch");
    deleteCell->setDetailTextColor(nvgRGB(220, 90, 90));
    deleteCell->registerClickAction([this](brls::View*) {
        confirmDelete();
        return true;
    });
    deleteCell->addGestureRecognizer(new brls::TapGestureRecognizer(deleteCell));
    content->addView(deleteCell);

    scroll->setContentView(content);

    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setWidthPercentage(100);
    root->setHeightPercentage(100);
    root->setFocusable(false);
    root->addView(scroll);

    auto* frame = new brls::AppletFrame(root);
    totk::ui::setCenteredHeaderTitle(frame, info.name.empty() ? "Edit Item" : info.name);
    // Attached to the frame itself, not root — see TabHostActivity's
    // matching comment (tab_host_activity.cpp) for why.
    totk::ui::attachEditorBackground(frame);

    refreshHeader();
    return frame;
}

void EquipmentEditActivity::refreshHeader() {
    auto& editor = AppState::instance().editor();
    const auto info = totk::displayInfoForPouchItem(editor, category_, static_cast<int>(index_));

    if (headerName_) headerName_->setText(info.name.empty() ? info.id : info.name);
    if (headerSubtitle_) headerSubtitle_->setText(info.subtitle);
    if (headerIcon_) {
        headerIcon_->clear();
        for (const auto& path : info.iconPaths) {
            if (totk::IconAtlas::instance().apply(headerIcon_, path)) break;
        }
    }
    if (headerFuseIcon_ && !info.fuseId.empty()) {
        headerFuseIcon_->clear();
        bool iconOk = false;
        for (const auto& path : info.fuseIconPaths) {
            if (totk::IconAtlas::instance().apply(headerFuseIcon_, path)) {
                iconOk = true;
                break;
            }
        }
        headerFuseIcon_->setVisibility(iconOk ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        if (headerFuseLabel_) {
            headerFuseLabel_->setText(iconOk ? "" : (info.fuseName.empty() ? info.fuseId : info.fuseName));
            headerFuseLabel_->setVisibility(iconOk ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
        }
    } else if (headerFuseLabel_) {
        if (headerFuseIcon_) {
            headerFuseIcon_->clear();
            headerFuseIcon_->setVisibility(brls::Visibility::GONE);
        }
        headerFuseLabel_->setText("None");
        headerFuseLabel_->setVisibility(brls::Visibility::VISIBLE);
    }
}

void EquipmentEditActivity::confirmDelete() {
    auto& editor = AppState::instance().editor();
    const auto info = totk::displayInfoForPouchItem(editor, category_, static_cast<int>(index_));
    const std::string prompt = "Remove \"" + (info.name.empty() ? "this item" : info.name) + "\" from your save?";

    const std::string category = category_;
    const size_t index = index_;
    auto* dialog = new brls::Dialog(prompt);
    dialog->addButton("Cancel", []() {});
    dialog->addButton("Delete", [category, index]() {
        // Mutate before popping, not after (same reasoning as
        // ItemPickerActivity::selectItem): TabHostActivity::onResume() rebuilds
        // the grid synchronously as part of the pop transition. Removing the item
        // afterward, in a callback deferred an extra frame past the pop, meant the
        // grid had already rebuilt from the still-stale equipment list by the time
        // the removal actually happened — the item stayed visible until the next
        // unrelated rebuild (tab switch, entering/leaving another item).
        auto& editor = AppState::instance().editor();
        const bool removed = editor.removeEquipmentItem(category, index);
        TOTK_LOG("editor: delete %s index=%zu removed=%d", category.c_str(), index, removed ? 1 : 0);
        if (!removed) {
            brls::Application::notify("Could not remove item");
            return;
        }
        brls::Application::popActivity(brls::TransitionAnimation::FADE);
    });
    dialog->open();
}

void EquipmentEditActivity::openFusePicker() {
    auto& db = totk::ItemDatabase::instance();
    const auto& fusible = db.fusibleItems();
    std::vector<totk::ItemEntry> items(fusible.begin(), fusible.end());

    brls::Application::pushActivity(
        ItemPickerActivity::forMixedGrid(
            "Fuse Material", std::move(items),
            [this](const std::string& fuseId) {
                // Runs synchronously, before the picker pops (see
                // ItemPickerActivity::selectItem) — safe here since this is a normal
                // main-thread input callback, not the async task loop.
                auto& editor = AppState::instance().editor();
                auto& items = editor.equipment()[category_];
                if (index_ >= items.size()) return;
                totk::GameLimits::applyEquipmentFuse(items[index_], fuseId);
                TOTK_LOG("editor: fuse %s index=%zu fuseId=%s", category_.c_str(), index_, fuseId.c_str());
                refreshHeader();
                if (fuseCell_) {
                    const std::string fuseName =
                        fuseId.empty() ? std::string() : totk::ItemDatabase::instance().nameForId(fuseId);
                    fuseCell_->setDetailText(fuseName.empty() ? "Choose material" : fuseName);
                }
            }),
        brls::TransitionAnimation::NONE);
}

}  // namespace totk::save_editor
