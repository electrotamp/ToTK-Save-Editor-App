#pragma once

#include <borealis.hpp>
#include <string>

namespace totk::ui {

class CompletionTab : public brls::ScrollingFrame {
public:
    CompletionTab();
    static brls::View* create();

private:
    void addCategory(brls::Box* content, const std::string& label, const std::string& unlockKey,
                     const std::string& pinCategory);
};

}  // namespace totk::ui
