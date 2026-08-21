#include "save_editor/save_editor_pouch.hpp"

#include "app/app_state.hpp"
#include "save/save_editor.hpp"
#include "ui/item_editor_ui.hpp"
#include "ui/pouch_item_display.hpp"
#include "ui/zonai_wisp_overlay.hpp"
#include "util/icon_atlas.hpp"
#include "util/totk_log.hpp"

#include <borealis/core/application.hpp>
#include <borealis/core/touch/tap_gesture.hpp>

namespace totk::save_editor {

PouchEditActivity::PouchEditActivity(std::string category, size_t index)
    : category_(std::move(category)), index_(index) {}

brls::View* PouchEditActivity::createContentView() {
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
    content->addView(headerRow);

    if (category_ == "armors") {
        auto& items = editor.armors();
        if (index_ < items.size()) {
            totk::ui::ItemEditorUi::addArmorEditor(content, items[index_], [this]() { refreshHeader(); });
        }
    } else if (category_ == "horses") {
        auto& items = editor.horses();
        if (index_ < items.size()) {
            totk::ui::ItemEditorUi::addHorseEditor(content, items[index_], [this]() { refreshHeader(); });
        }
    } else {
        auto& items = editor.stackItems()[category_];
        if (index_ < items.size()) {
            if (category_ == "food") {
                totk::ui::ItemEditorUi::addFoodEditor(content, items[index_]);
            } else {
                totk::ui::ItemEditorUi::addStackEditor(content, items[index_]);
            }
        }
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

void PouchEditActivity::refreshHeader() {
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
}

void PouchEditActivity::confirmDelete() {
    auto& editor = AppState::instance().editor();
    const auto info = totk::displayInfoForPouchItem(editor, category_, static_cast<int>(index_));
    const std::string prompt = "Remove \"" + (info.name.empty() ? "this item" : info.name) + "\" from your save?";

    const std::string category = category_;
    const size_t index = index_;
    auto* dialog = new brls::Dialog(prompt);
    dialog->addButton("Cancel", []() {});
    dialog->addButton("Delete", [category, index]() {
        // Mutate before popping — same reasoning as EquipmentEditActivity::confirmDelete.
        auto& editor = AppState::instance().editor();
        bool removed = false;
        if (category == "armors") {
            removed = editor.removeArmorItem(index);
        } else if (category == "horses") {
            removed = editor.removeHorseItem(index);
        } else {
            removed = editor.removeStackItem(category, index);
        }
        TOTK_LOG("editor: delete %s index=%zu removed=%d", category.c_str(), index, removed ? 1 : 0);
        if (!removed) {
            brls::Application::notify("Could not remove item");
            return;
        }
        brls::Application::popActivity(brls::TransitionAnimation::FADE);
    });
    dialog->open();
}

}  // namespace totk::save_editor
