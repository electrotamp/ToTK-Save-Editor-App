#pragma once

#include <borealis.hpp>

namespace totk::ui {

class ItemCell : public brls::RecyclerCell {
public:
    ItemCell();
    void setItem(const std::string& title, const std::string& subtitle,
                 const std::vector<std::string>& iconPaths);
    static ItemCell* create();

    BRLS_BIND(brls::Label, title, "title");
    BRLS_BIND(brls::Label, subtitle, "subtitle");
    BRLS_BIND(brls::Image, icon, "icon");
};

}  // namespace totk::ui
