#include "ui/item_edit_activity.hpp"

#include "app/app_state.hpp"
#include "save/game_limits.hpp"
#include "save/save_editor.hpp"
#include "ui/item_detail_header.hpp"
#include "ui/editor_layout.hpp"
#include "ui/item_database.hpp"
#include "ui/item_editor_ui.hpp"
#include "ui/item_picker_activity.hpp"
#include "ui/pouch_item_display.hpp"
#include "util/totk_log.hpp"

namespace totk::ui {

ItemEditActivity::ItemEditActivity(std::string category, int index, std::function<void(int deletedIndex)> onDeleted,
                                   std::function<void()> onClosed)
    : category_(std::move(category)),
      index_(index),
      onDeleted_(std::move(onDeleted)),
      onClosed_(std::move(onClosed)) {}

ItemEditActivity::~ItemEditActivity() {
    if (onClosed_) {
        const auto onClosed = std::move(onClosed_);
        brls::sync([onClosed]() { onClosed(); });
    }
}

brls::View* ItemEditActivity::createContentView() {
    auto& editor = AppState::instance().editor();
    const auto info = totk::displayInfoForPouchItem(editor, category_, index_);

    auto* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1.0f);

    auto* content = new brls::Box();
    content->setAxis(brls::Axis::COLUMN);
    content->setWidthPercentage(100);
    content->setPadding(kEditorPagePaddingTop, kEditorPageInset, kEditorPagePaddingBottom, kEditorPageInset);

    auto* hint = new brls::Label();
    hint->setText("Edit values below. Press B to return to the item grid.");
    hint->setFontSize(15);
    hint->setTextColor(nvgRGB(160, 160, 160));
    hint->setMarginBottom(10);
    content->addView(hint);

    header_ = new ItemFocusHeader();
    header_->configure(info, 72.0f);
    header_->setMarginBottom(12);
    content->addView(header_);

    if (category_ == "weapons" || category_ == "bows" || category_ == "shields") {
        auto& item = editor.equipment()[category_][static_cast<size_t>(index_)];
        const bool withFuse = category_ == "weapons" || category_ == "shields";
        fuseCell_ = ItemEditorUi::addEquipmentEditor(
            content, item, withFuse, true,
            withFuse ? [this]() { openFusePicker(); } : std::function<void()>());
    } else if (category_ == "armors") {
        ItemEditorUi::addArmorEditor(content, editor.armors()[static_cast<size_t>(index_)],
                                     [this]() { refreshArmorDisplay(); });
    } else if (category_ == "food") {
        ItemEditorUi::addFoodEditor(content, editor.stackItems()["food"][static_cast<size_t>(index_)]);
    } else if (category_ == "horses") {
        ItemEditorUi::addHorseEditor(content, editor.horses()[static_cast<size_t>(index_)],
                                     [this]() { refreshHorseDisplay(); });
    } else {
        ItemEditorUi::addStackEditor(content, editor.stackItems()[category_][static_cast<size_t>(index_)]);
    }

    auto* deleteCell = new brls::DetailCell();
    deleteCell->setText("Delete Item");
    deleteCell->setDetailText("Remove from pouch");
    deleteCell->setDetailTextColor(nvgRGB(220, 90, 90));
    deleteCell->registerClickAction([this](brls::View*) {
        confirmDelete();
        return true;
    });
    content->addView(deleteCell);

    scroll->setContentView(content);

    auto* frame = new brls::AppletFrame(scroll);
    frame->setTitle(info.name.empty() ? "Edit Item" : info.name);
    return frame;
}

void ItemEditActivity::confirmDelete() {
    auto& saveEditor = AppState::instance().editor();
    const auto info = totk::displayInfoForPouchItem(saveEditor, category_, index_);
    const std::string prompt = "Remove \"" + (info.name.empty() ? "this item" : info.name) + "\" from your save?";

    const std::string category = category_;
    const int index = index_;
    const auto onDeleted = onDeleted_;

    brls::Dialog* dialog = new brls::Dialog(prompt);
    dialog->addButton("Cancel", []() {});
    dialog->addButton("Delete", [category, index, onDeleted, info]() {
        brls::Application::popActivity(brls::TransitionAnimation::FADE, [category, index, onDeleted, info]() {
            brls::sync([category, index, onDeleted, info]() {
                auto& editor = AppState::instance().editor();
                const size_t itemIndex = static_cast<size_t>(index);
                bool removed = false;

                if (category == "weapons" || category == "bows" || category == "shields") {
                    removed = editor.removeEquipmentItem(category, itemIndex);
                } else if (category == "armors") {
                    removed = editor.removeArmorItem(itemIndex);
                } else if (category == "horses") {
                    removed = editor.removeHorseItem(itemIndex);
                } else {
                    removed = editor.removeStackItem(category, itemIndex);
                }

                TOTK_ACTION("delete_item %s index=%d removed=%d", category.c_str(), index, removed ? 1 : 0);

                if (!removed) {
                    brls::Application::notify("Could not remove item");
                    return;
                }

                if (onDeleted) onDeleted(index);
                brls::Application::notify("Removed " + (info.name.empty() ? "item" : info.name));
            });
        });
    });
    dialog->open();
}

void ItemEditActivity::refreshEquipmentDisplay() {
    auto& editor = AppState::instance().editor();
    const auto info = totk::displayInfoForPouchItem(editor, category_, index_);

    if (header_) {
        header_->configure(info, 72.0f);
    }

    if (fuseCell_) {
        const std::string fuseName = info.fuseId.empty() ? std::string() : info.fuseName;
        fuseCell_->setDetailText(fuseName.empty() ? "Choose material" : fuseName);
    }
}

void ItemEditActivity::refreshHorseDisplay() {
    auto& editor = AppState::instance().editor();
    const auto info = totk::displayInfoForPouchItem(editor, category_, index_);
    if (header_) {
        header_->configure(info, 72.0f);
    }
}

void ItemEditActivity::refreshArmorDisplay() {
    auto& editor = AppState::instance().editor();
    const auto info = totk::displayInfoForPouchItem(editor, category_, index_);
    if (header_) {
        header_->configure(info, 72.0f);
    }
}

void ItemEditActivity::openFusePicker() {
    if (category_ != "weapons" && category_ != "shields") return;

    brls::Application::pushActivity(ItemPickerActivity::fusePicker([this](const std::string& fuseId) {
        auto& editor = AppState::instance().editor();
        auto& item = editor.equipment()[category_][static_cast<size_t>(index_)];
        totk::GameLimits::applyEquipmentFuse(item, fuseId);
        refreshEquipmentDisplay();
    }), brls::TransitionAnimation::NONE);
}

}  // namespace totk::ui
