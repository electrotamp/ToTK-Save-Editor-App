#include "ui/item_cell.hpp"

#include "util/image_loader.hpp"
#include "util/totk_log.hpp"

namespace totk::ui {

ItemCell::ItemCell() {
    this->inflateFromXMLRes("xml/cells/item_cell.xml");
}

void ItemCell::setItem(const std::string& titleText, const std::string& subtitleText,
                       const std::vector<std::string>& iconPaths) {
    try {
        title->setText(titleText);
        subtitle->setText(subtitleText);
    } catch (const std::exception& ex) {
        TOTK_LOG("ItemCell labels failed: %s", ex.what());
        return;
    }

    if (iconPaths.empty() || !totk::loadRomfsImageFirst(icon, iconPaths)) {
        icon->clear();
        icon->setVisibility(brls::Visibility::GONE);
        return;
    }

    icon->setVisibility(brls::Visibility::VISIBLE);
}

ItemCell* ItemCell::create() { return new ItemCell(); }

}  // namespace totk::ui
