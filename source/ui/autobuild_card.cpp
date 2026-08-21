#include "ui/autobuild_card.hpp"

#include <algorithm>

#include <borealis/core/application.hpp>
#include <borealis/core/thread.hpp>

namespace totk::ui {

namespace {

constexpr float kCardWidth = 380.0f;
constexpr float kThumbWidth = 360.0f;
constexpr float kThumbHeight = 203.0f;  // ~16:9, matches the console's own 320x180 Draft icons
constexpr float kCardMargin = 8.0f;
constexpr float kCardPadding = 10.0f;
constexpr float kCardCornerRadius = 8.0f;

}  // namespace

void buildAutobuildCardGrid(brls::Box* content, int columns, const std::vector<AutobuildCardSpec>& cards,
                             std::vector<brls::Image*>* outThumbs) {
    if (!content || columns <= 0) return;

    for (size_t i = 0; i < cards.size(); ++i) {
        if (i % static_cast<size_t>(columns) == 0) {
            auto* rowBox = new brls::Box();
            rowBox->setAxis(brls::Axis::ROW);
            rowBox->setFocusable(false);
            content->addView(rowBox);
        }

        auto* rowBox = dynamic_cast<brls::Box*>(content->getChildren().back());
        if (!rowBox) continue;

        const auto& spec = cards[i];

        auto* card = new brls::Box();
        card->setAxis(brls::Axis::COLUMN);
        card->setWidth(kCardWidth);
        card->setMargins(kCardMargin, kCardMargin, kCardMargin, kCardMargin);
        card->setPadding(kCardPadding);
        card->setCornerRadius(kCardCornerRadius);
        card->setBackgroundColor(nvgRGBA(255, 255, 255, 16));
        card->setFocusable(static_cast<bool>(spec.onSelect));

        auto* thumb = new brls::Image();
        thumb->setFocusable(false);
        thumb->setWidth(kThumbWidth);
        thumb->setHeight(kThumbHeight);
        thumb->setScalingType(brls::ImageScalingType::FILL);
        thumb->setCornerRadius(kCardCornerRadius * 0.6f);
        thumb->setMarginBottom(8);
        card->addView(thumb);
        // Blank tile (not skipped) when there's nothing to show, so every
        // card in the row stays the same height/alignment.
        if (spec.loadIcon) spec.loadIcon(thumb);
        if (outThumbs) outThumbs->push_back(thumb);

        auto* title = new brls::Label();
        title->setFocusable(false);
        title->setFontSize(17);
        title->setText(spec.title);
        card->addView(title);

        auto* status = new brls::Label();
        status->setFocusable(false);
        status->setFontSize(14);
        status->setTextColor(spec.statusColor);
        status->setText(spec.statusText);
        card->addView(status);

        if (spec.onSelect) {
            auto onSelect = spec.onSelect;
            card->registerAction(
                "Select", brls::BUTTON_A, [onSelect](brls::View*) { onSelect(); return true; }, false, true);
            card->addGestureRecognizer(new brls::TapGestureRecognizer(card));
        }

        rowBox->addView(card);
    }
}

namespace {

void applyChunk(std::vector<brls::Image*> thumbs, std::vector<std::function<bool(brls::Image*)>> loaders,
                std::function<bool()> stillValid, int perFrame, size_t index) {
    if (stillValid && !stillValid()) return;

    const size_t end = std::min(thumbs.size(), index + static_cast<size_t>(perFrame));
    for (size_t i = index; i < end; ++i) {
        if (i < loaders.size() && loaders[i] && thumbs[i]) loaders[i](thumbs[i]);
    }

    if (end < thumbs.size()) {
        brls::sync([thumbs, loaders, stillValid, perFrame, end]() {
            applyChunk(thumbs, loaders, stillValid, perFrame, end);
        });
    }
}

}  // namespace

void applyAutobuildIconsStaggered(std::vector<brls::Image*> thumbs,
                                   std::vector<std::function<bool(brls::Image*)>> loaders,
                                   std::function<bool()> stillValid, int perFrame) {
    applyChunk(std::move(thumbs), std::move(loaders), std::move(stillValid), perFrame, 0);
}

}  // namespace totk::ui
