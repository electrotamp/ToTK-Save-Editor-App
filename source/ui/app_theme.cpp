#include "ui/app_theme.hpp"

#include <borealis/core/application.hpp>
#include <borealis/core/box.hpp>

#include "ui/item_detail_header.hpp"
#include "ui/item_icon_tile.hpp"
#include "ui/status_tab.hpp"

namespace totk::ui {

namespace {

void refreshThemedViewsRecursive(brls::View* view) {
    if (!view) return;

    if (auto* tile = dynamic_cast<ItemIconTile*>(view)) {
        tile->applyThemeColors();
    } else if (auto* addTile = dynamic_cast<ItemAddTile*>(view)) {
        addTile->applyThemeColors();
    } else if (auto* header = dynamic_cast<ItemFocusHeader*>(view)) {
        header->applyThemeColors();
    } else if (auto* statusTab = dynamic_cast<StatusTab*>(view)) {
        statusTab->applyThemeColors();
    }

    if (auto* box = dynamic_cast<brls::Box*>(view)) {
        for (brls::View* child : box->getChildren()) {
            refreshThemedViewsRecursive(child);
        }
    }
}

}  // namespace

AppTheme& AppTheme::instance() {
    static AppTheme theme;
    return theme;
}

void AppTheme::setColors(AppThemeColors colors) {
    colors_ = colors;
}

void AppTheme::refreshThemedViews() {
    for (brls::Activity* activity : brls::Application::getActivitiesStack()) {
        refreshThemedViewsRecursive(activity->getContentView());
    }
}

}  // namespace totk::ui