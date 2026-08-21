#include "ui/autobuild_import_activity.hpp"

#include "app/app_state.hpp"
#include "save/autobuilder_cai.hpp"
#include "save/save_editor.hpp"
#include "ui/autobuild_slot_picker.hpp"
#include "util/totk_log.hpp"

#include <borealis/core/application.hpp>
#include <borealis/core/box.hpp>
#include <borealis/views/applet_frame.hpp>
#include <borealis/views/label.hpp>
#include <borealis/views/scrolling_frame.hpp>

namespace totk::ui {

AutobuildImportActivity::AutobuildImportActivity(std::string startDirectory)
    : currentDirectory_(std::move(startDirectory)) {}

brls::View* AutobuildImportActivity::createContentView() {
    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setWidthPercentage(100);
    root->setHeightPercentage(100);
    root->setPadding(16, 16, 16, 16);

    pathLabel_ = new brls::Label();
    pathLabel_->setFocusable(false);
    pathLabel_->setFontSize(14);
    pathLabel_->setMarginBottom(8);
    root->addView(pathLabel_);

    listFrame_ = new brls::ScrollingFrame();
    listFrame_->setGrow(1.0f);
    listFrame_->setWidthPercentage(100);

    listBox_ = new brls::Box();
    listBox_->setAxis(brls::Axis::COLUMN);
    listBox_->setWidthPercentage(100);
    listFrame_->setContentView(listBox_);
    root->addView(listFrame_);

    root->registerAction(
        "Back", brls::BUTTON_B,
        [](brls::View*) {
            brls::Application::popActivity(brls::TransitionAnimation::FADE);
            return true;
        },
        false, true);

    auto* frame = new brls::AppletFrame(root);
    frame->setTitle("Import Autobuild (.cai)");

    openDirectory(currentDirectory_);
    return frame;
}

void AutobuildImportActivity::openDirectory(const std::string& path) {
    currentDirectory_ = path;
    if (pathLabel_) pathLabel_->setText(path);

    if (!listBox_) return;
    listBox_->clearViews(false);

    const auto entries = totk::listDirectoryEntries(path, ".cai");
    if (entries.empty()) {
        auto* empty = new brls::Label();
        empty->setFocusable(false);
        empty->setText("No .cai blueprint files found here.");
        listBox_->addView(empty);
        return;
    }

    for (const auto& entry : entries) {
        auto* row = new brls::Box();
        row->setAxis(brls::Axis::ROW);
        row->setWidthPercentage(100);
        row->setMarginBottom(8);
        row->setFocusable(true);
        row->setPadding(12, 12, 12, 12);
        row->setCornerRadius(4);

        auto* label = new brls::Label();
        label->setFocusable(false);
        label->setGrow(1.0f);
        label->setText(entry.isDirectory ? entry.name + "/" : entry.name);
        row->addView(label);

        row->registerAction(
            "Open", brls::BUTTON_A,
            [this, entry](brls::View*) {
                openEntry(entry);
                return true;
            },
            false, true);
        row->addGestureRecognizer(new brls::TapGestureRecognizer(row));

        listBox_->addView(row);
    }

    for (brls::View* child : listBox_->getChildren()) {
        if (child && child->isFocusable()) {
            brls::Application::giveFocus(child);
            return;
        }
    }
}

void AutobuildImportActivity::openEntry(const totk::FileBrowserEntry& entry) {
    if (entry.isDirectory) {
        openDirectory(entry.fullPath);
        return;
    }
    importFile(entry.fullPath);
}

void AutobuildImportActivity::importFile(const std::string& fullPath) {
    const std::string content = totk::loadFileText(fullPath);
    if (content.empty()) {
        brls::Application::notify("Could not read that file");
        return;
    }

    std::vector<uint8_t> bytes(content.begin(), content.end());
    auto blueprint = std::make_shared<totk::AutobuilderCai>();
    std::string parseError;
    if (!totk::parseAutobuilderCai(bytes, *blueprint, parseError)) {
        TOTK_LOG("autobuild import: parse failed for %s: %s", fullPath.c_str(), parseError.c_str());
        brls::Application::notify(parseError);
        return;
    }

    pushAutobuildSlotPicker([blueprint](totk::ui::AutobuildImportTarget target) {
        auto& editor = AppState::instance().editor();
        std::string error;
        size_t draftPosition = 0;
        const bool ok = target.isFavorite
            ? editor.importAutobuilderFavorite(target.favoriteIndex, *blueprint, error, &draftPosition)
            : editor.importAutobuilderHistory(target.historyDraftPosition, *blueprint, error, &draftPosition);
        if (ok) {
            editor.saveAutobuilder();
            (void)draftPosition;
        }

        brls::Application::popActivity(brls::TransitionAnimation::FADE, [ok, error, target]() {
            std::string message;
            if (ok) {
                message = target.isFavorite ? "Imported to Favorite " + std::to_string(target.favoriteIndex + 1)
                                             : "Imported to History";
            } else {
                message = error;
            }
            brls::Application::notify(message);
        });
    });
}

void pushAutobuildImportActivity() {
    brls::Application::pushActivity(new AutobuildImportActivity("sdmc:/"), brls::TransitionAnimation::FADE);
}

}  // namespace totk::ui
