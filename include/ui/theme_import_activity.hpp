#pragma once

#include <functional>
#include <string>

#include <borealis.hpp>

#include "util/file_text.hpp"

namespace totk::ui {

class ThemeImportActivity : public brls::Activity {
public:
    ThemeImportActivity(std::string startDirectory, std::function<void(bool success, const std::string& message)> onFinished);

    brls::View* createContentView() override;

private:
    void openDirectory(const std::string& path);
    void openEntry(const totk::FileBrowserEntry& entry);

    std::string currentDirectory_;
    std::function<void(bool success, const std::string& message)> onFinished_;
    brls::ScrollingFrame* listFrame_ = nullptr;
    brls::Box* listBox_ = nullptr;
    brls::Label* pathLabel_ = nullptr;
};

void pushThemeImportPicker(std::function<void(bool success, const std::string& message)> onFinished);

}  // namespace totk::ui
