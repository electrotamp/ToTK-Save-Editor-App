#include "ui/item_detail_header.hpp"

#include "ui/app_theme.hpp"
#include "util/image_loader.hpp"

namespace totk::ui {

namespace {

void refreshImageIfLaidOut(brls::Image* image) {
    if (!image || image->getTexture() == 0) return;
    if (image->getWidth() > 1.0f && image->getHeight() > 1.0f) {
        image->invalidate();
    }
}

}  // namespace

ItemFocusHeader::ItemFocusHeader() {
    this->setAxis(brls::Axis::ROW);
    this->setWidthPercentage(100);
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setFocusable(false);
    this->setShrink(0.0f);

    baseIcon_ = new brls::Image();
    baseIcon_->setScalingType(brls::ImageScalingType::FIT);
    baseIcon_->setShrink(0.0f);
    baseIcon_->setGrow(0.0f);
    baseIcon_->setFocusable(false);
    baseIcon_->setMarginRight(12);
    this->addView(baseIcon_);

    auto* textColumn = new brls::Box();
    textColumn->setAxis(brls::Axis::COLUMN);
    textColumn->setGrow(1.0f);
    textColumn->setShrink(1.0f);
    textColumn->setFocusable(false);

    name_ = new brls::Label();
    name_->setSingleLine(true);
    name_->setWidthPercentage(100);
    name_->setMarginBottom(4);
    name_->setFocusable(false);
    textColumn->addView(name_);

    subtitle_ = new brls::Label();
    subtitle_->setSingleLine(true);
    subtitle_->setWidthPercentage(100);
    subtitle_->setFocusable(false);
    textColumn->addView(subtitle_);

    this->addView(textColumn);

    auto* fuseColumn = new brls::Box();
    fuseColumn->setAxis(brls::Axis::COLUMN);
    fuseColumn->setAlignItems(brls::AlignItems::CENTER);
    fuseColumn->setShrink(0.0f);
    fuseColumn->setGrow(0.0f);
    fuseColumn->setMarginLeft(12);
    fuseColumn->setFocusable(false);

    fuseTitle_ = new brls::Label();
    fuseTitle_->setText("Fuse");
    fuseTitle_->setFontSize(13);
    fuseTitle_->setMarginBottom(4);
    fuseTitle_->setFocusable(false);
    fuseColumn->addView(fuseTitle_);

    fusePanel_ = new brls::Box();
    fusePanel_->setAxis(brls::Axis::COLUMN);
    fusePanel_->setAlignItems(brls::AlignItems::CENTER);
    fusePanel_->setJustifyContent(brls::JustifyContent::CENTER);
    fusePanel_->setShrink(0.0f);
    fusePanel_->setGrow(0.0f);
    fusePanel_->setCornerRadius(6);
    fusePanel_->setFocusable(false);
    fusePanel_->setVisibility(brls::Visibility::GONE);
    fuseColumn->addView(fusePanel_);

    fuseIcon_ = new brls::Image();
    fuseIcon_->setScalingType(brls::ImageScalingType::FIT);
    fuseIcon_->setShrink(0.0f);
    fuseIcon_->setGrow(0.0f);
    fuseIcon_->setFocusable(false);
    fuseIcon_->setVisibility(brls::Visibility::GONE);
    fusePanel_->addView(fuseIcon_);

    fuseNone_ = new brls::Label();
    fuseNone_->setText("None");
    fuseNone_->setFontSize(14);
    fuseNone_->setSingleLine(true);
    fuseNone_->setFocusable(false);
    fuseNone_->setVisibility(brls::Visibility::GONE);
    fusePanel_->addView(fuseNone_);

    this->addView(fuseColumn);
    applyThemeColors();
}

void ItemFocusHeader::applyThemeColors() {
    const auto& colors = AppTheme::instance().colors();
    subtitle_->setTextColor(colors.mutedText);
    fuseTitle_->setTextColor(colors.mutedText);
    fusePanel_->setBackgroundColor(colors.fusePanelBackground);
    fuseNone_->setTextColor(colors.mutedText);
}

void ItemFocusHeader::configure(const totk::ItemDisplayInfo& info, float iconSize) {
    const float largeText = iconSize >= 64.0f;

    baseIcon_->setWidth(iconSize);
    baseIcon_->setHeight(iconSize);
    baseIcon_->clear();
    if (!totk::loadRomfsImageFirst(baseIcon_, info.iconPaths)) {
        baseIcon_->clear();
    } else {
        refreshImageIfLaidOut(baseIcon_);
    }

    name_->setText(info.name.empty() ? info.id : info.name);
    name_->setFontSize(largeText ? 24 : 22);

    if (!info.subtitle.empty()) {
        subtitle_->setText(info.subtitle);
        subtitle_->setFontSize(largeText ? 17 : 16);
        subtitle_->setVisibility(brls::Visibility::VISIBLE);
    } else {
        subtitle_->setText("");
        subtitle_->setVisibility(brls::Visibility::GONE);
    }

    if (!info.showFusePanel && !info.showTackPanel) {
        fuseTitle_->setVisibility(brls::Visibility::GONE);
        fusePanel_->setVisibility(brls::Visibility::GONE);
        return;
    }

    const bool isTack = info.showTackPanel;
    fuseTitle_->setText(isTack ? "Tack" : "Fuse");
    fuseTitle_->setVisibility(brls::Visibility::VISIBLE);
    fuseTitle_->setFontSize(largeText ? 13 : 12);

    fusePanel_->setVisibility(brls::Visibility::VISIBLE);
    fusePanel_->setWidth(iconSize);
    fusePanel_->setHeight(iconSize);
    fusePanel_->setPadding(0);

    fuseIcon_->setWidth(iconSize);
    fuseIcon_->setHeight(iconSize);

    fuseNone_->setFontSize(largeText ? 16 : 14);
    fuseNone_->setWidth(iconSize);
    fuseNone_->setHeight(iconSize);
    fuseNone_->setHorizontalAlign(brls::HorizontalAlign::CENTER);

    const std::vector<std::string>& iconPaths = isTack ? info.tackIconPaths : info.fuseIconPaths;
    const std::string& displayName = isTack ? info.tackName : info.fuseName;
    const bool hasAccessory = isTack ? !info.tackName.empty() && info.tackName != "None"
                                     : !info.fuseId.empty();
    if (hasAccessory && !iconPaths.empty() && totk::loadRomfsImageFirst(fuseIcon_, iconPaths)) {
        fuseIcon_->setVisibility(brls::Visibility::VISIBLE);
        fuseNone_->setVisibility(brls::Visibility::GONE);
        refreshImageIfLaidOut(fuseIcon_);
        return;
    }

    fuseIcon_->clear();
    fuseIcon_->setVisibility(brls::Visibility::GONE);
    fuseNone_->setText(hasAccessory && !displayName.empty() ? displayName : "None");
    fuseNone_->setVisibility(brls::Visibility::VISIBLE);
}

brls::Box* createItemDetailHeader(const totk::ItemDisplayInfo& info, float iconSize) {
    auto* header = new ItemFocusHeader();
    header->configure(info, iconSize);
    return header;
}

}  // namespace totk::ui
