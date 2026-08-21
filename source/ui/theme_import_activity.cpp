#include "ui/theme_import_activity.hpp"

#include "ui/editor_theme.hpp"

#include <borealis/core/application.hpp>
#include <borealis/core/box.hpp>
#include <borealis/views/applet_frame.hpp>
#include <borealis/views/label.hpp>
#include <borealis/views/scrolling_frame.hpp>

namespace totk::ui {

ThemeImportActivity::ThemeImportActivity(std::string startDirectory,
                                         std::function<void(bool success, const std::string& message)> onFinished)
    : currentDirectory_(std::move(startDirectory)), onFinished_(std::move(onFinished)) {}

brls::View* ThemeImportActivity::createContentView() {
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
        [this](brls::View*) {
            if (onFinished_) onFinished_(false, "Theme import cancelled");
            brls::Application::popActivity(brls::TransitionAnimation::FADE);
            return true;
        },
        false, true);

    auto* frame = new brls::AppletFrame(root);
    frame->setTitle("Import editor theme");

    openDirectory(currentDirectory_);
    return frame;
}

void ThemeImportActivity::openDirectory(const std::string& path) {
    currentDirectory_ = path;
    if (pathLabel_) pathLabel_->setText(path);

    if (!listBox_) return;
    listBox_->clearViews(false);

    const auto entries = totk::listDirectoryEntries(path, ".json");
    if (entries.empty()) {
        auto* empty = new brls::Label();
        empty->setFocusable(false);
        empty->setText("No theme JSON found in this folder.\nBrowse to the folder where you keep your theme file.");
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
        if (entry.isDirectory) {
            label->setText(entry.name + "/");
        } else {
            label->setText(entry.name);
        }
        row->addView(label);

        row->registerAction(
            "Open", brls::BUTTON_A,
            [this, entry](brls::View*) {
                openEntry(entry);
                return true;
            },
            false, true);

        listBox_->addView(row);
    }

    for (brls::View* child : listBox_->getChildren()) {
        if (child && child->isFocusable()) {
            brls::Application::giveFocus(child);
            return;
        }
    }
}

void ThemeImportActivity::openEntry(const totk::FileBrowserEntry& entry) {
    if (entry.isDirectory) {
        openDirectory(entry.fullPath);
        return;
    }

    if (EditorTheme::instance().loadFromFile(entry.fullPath)) {
        if (onFinished_) {
            onFinished_(true, "Applied theme: " + EditorTheme::instance().name());
        }
        brls::Application::popActivity(brls::TransitionAnimation::FADE);
        return;
    }

    brls::Application::notify("Failed to load theme JSON");
}

void pushThemeImportPicker(std::function<void(bool success, const std::string& message)> onFinished) {
    brls::Application::pushActivity(new ThemeImportActivity(EditorTheme::kImportDirectory, std::move(onFinished)),
                                    brls::TransitionAnimation::FADE);
}

}  // namespace totk::ui
