#pragma once

#include <string>

#include <borealis.hpp>

#include "util/file_text.hpp"

namespace totk::ui {

// SD-card ".cai" blueprint browser — same directory-browse shape as
// ThemeImportActivity, filtered to ".cai" instead of ".json". Selecting a
// file parses it, then hands off to pushAutobuildSlotPicker() to choose the
// target Favorites slot before actually writing into the save.
class AutobuildImportActivity : public brls::Activity {
public:
    explicit AutobuildImportActivity(std::string startDirectory);

    brls::View* createContentView() override;

private:
    void openDirectory(const std::string& path);
    void openEntry(const totk::FileBrowserEntry& entry);
    void importFile(const std::string& fullPath);

    std::string currentDirectory_;
    brls::ScrollingFrame* listFrame_ = nullptr;
    brls::Box* listBox_ = nullptr;
    brls::Label* pathLabel_ = nullptr;
};

// Pushes the SD import browser rooted at the SD card root.
void pushAutobuildImportActivity();

}  // namespace totk::ui
