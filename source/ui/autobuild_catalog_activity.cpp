#include "ui/autobuild_catalog_activity.hpp"

#include "net/http_client.hpp"
#include "ui/autobuild_card.hpp"
#include "ui/autobuild_detail_activity.hpp"
#include "ui/zonai_wisp_overlay.hpp"
#include "util/image_loader.hpp"
#include "util/totk_log.hpp"

#include <borealis/core/application.hpp>
#include <borealis/core/box.hpp>
#include <borealis/views/applet_frame.hpp>
#include <borealis/views/image.hpp>
#include <borealis/views/label.hpp>
#include <borealis/views/scrolling_frame.hpp>

#include <functional>

namespace totk::ui {

namespace {
constexpr int kGridColumns = 3;
}  // namespace

AutobuildCatalogActivity::AutobuildCatalogActivity() = default;

brls::View* AutobuildCatalogActivity::createContentView() {
    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setWidthPercentage(100);
    root->setHeightPercentage(100);
    root->setPadding(16, 16, 16, 16);
    root_ = root;

    statusLabel_ = new brls::Label();
    statusLabel_->setFocusable(false);
    statusLabel_->setFontSize(15);
    statusLabel_->setTextColor(nvgRGB(160, 160, 160));
    statusLabel_->setMarginBottom(8);
    root->addView(statusLabel_);

    listFrame_ = new brls::ScrollingFrame();
    listFrame_->setGrow(1.0f);
    listFrame_->setWidthPercentage(100);
    listFrame_->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    listFrame_->setFocusable(false);

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
    root->registerAction(
        "Search", brls::BUTTON_X, [this](brls::View*) { openSearch(); return true; }, false, true);
    root->registerAction(
        "Prev Page", brls::BUTTON_LB,
        [this](brls::View*) {
            if (page_ > 1) loadPage(page_ - 1);
            return true;
        },
        false, true);
    root->registerAction(
        "Next Page", brls::BUTTON_RB, [this](brls::View*) { loadPage(page_ + 1); return true; }, false, true);

    auto* frame = new brls::AppletFrame(root);
    totk::ui::setCenteredHeaderTitle(frame, "HyruleWorks Catalog");
    // Same wallpaper/wisp treatment as every other editor screen — attached
    // to the frame itself, not root, so it covers the header/footer too.
    totk::ui::attachEditorBackground(frame);

    loadPage(1);
    return frame;
}

void AutobuildCatalogActivity::loadPage(int page) {
    page_ = page;
    ++loadGeneration_;
    const int generation = loadGeneration_;
    hasNextPage_ = false;
    if (root_) {
        root_->setActionAvailable(brls::BUTTON_LB, page_ > 1);
        root_->setActionAvailable(brls::BUTTON_RB, false);
    }

    if (statusLabel_) {
        statusLabel_->setText("Loading page " + std::to_string(page) + "...");
    }
    if (listBox_) listBox_->clearViews(false);
    rowImages_.clear();

    const std::string query = searchQuery_;
    brls::async([this, page, query, generation]() {
        std::vector<totk::net::HyruleWorksBuildSummary> builds;
        bool hasNextPage = false;
        std::string error;
        const bool ok = totk::net::fetchHyruleWorksCatalogPage(page, query, builds, hasNextPage, error);
        brls::sync([this, generation, ok, builds, hasNextPage, error]() {
            if (generation != loadGeneration_) return;  // superseded by a newer page/search request
            hasNextPage_ = ok && hasNextPage;
            if (root_) root_->setActionAvailable(brls::BUTTON_RB, hasNextPage_);
            // Builds with no attached blueprint can't be imported at all —
            // hide them rather than showing a dead-end card, per explicit
            // request.
            builds_.clear();
            for (auto& build : builds) {
                if (!build.caiFileUrl.empty()) builds_.push_back(build);
            }
            if (!ok) {
                if (statusLabel_) statusLabel_->setText(error.empty() ? "Failed to load catalog." : error);
                return;
            }
            renderBuilds();
        });
    });
}

void AutobuildCatalogActivity::renderBuilds() {
    if (!listBox_) return;
    listBox_->clearViews(false);
    rowImages_.clear();

    if (statusLabel_) {
        std::string status = "Page " + std::to_string(page_);
        if (!searchQuery_.empty()) status += " — search: \"" + searchQuery_ + "\"";
        if (builds_.empty()) status += " — no importable builds found";
        statusLabel_->setText(status);
    }

    std::vector<AutobuildCardSpec> cards;
    cards.reserve(builds_.size());
    for (const auto& build : builds_) {
        AutobuildCardSpec spec;
        spec.title = build.name.empty() ? "(untitled)" : build.name;
        spec.statusText = "by " + (build.creator.empty() ? std::string("unknown") : build.creator);
        spec.statusColor = nvgRGB(160, 160, 160);

        const int buildId = build.id;
        spec.onSelect = [buildId]() { pushAutobuildDetailActivity(buildId); };
        cards.push_back(std::move(spec));
    }
    buildAutobuildCardGrid(listBox_, kGridColumns, cards, &rowImages_);

    brls::View* firstFocusable = nullptr;
    for (brls::View* row : listBox_->getChildren()) {
        auto* rowBox = dynamic_cast<brls::Box*>(row);
        if (!rowBox) continue;
        for (brls::View* card : rowBox->getChildren()) {
            if (card && card->isFocusable()) {
                firstFocusable = card;
                break;
            }
        }
        if (firstFocusable) break;
    }
    if (firstFocusable) brls::Application::giveFocus(firstFocusable);

    loadThumbnails(loadGeneration_);
}

void AutobuildCatalogActivity::loadThumbnails(int generation) {
    // Fetched concurrently on a shared libcurl multi-handle (see
    // HttpClient::getMany) instead of one full connection+TLS handshake at a
    // time — the old one-request-at-a-time recursive load was the actual
    // cause of "icons load oddly slow", not image size or decode cost.
    std::vector<std::string> urls;
    urls.reserve(builds_.size());
    for (const auto& build : builds_) {
        urls.push_back(build.imageUrl.empty() ? std::string() : totk::net::hyruleWorksImageCdnUrl(build.imageUrl, 200));
    }

    brls::async([this, generation, urls]() {
        const std::vector<totk::net::HttpResponse> responses = totk::net::HttpClient::getMany(urls);
        brls::sync([this, generation, responses]() {
            if (generation != loadGeneration_) return;

            // Decode/upload staggered across frames, not all in this one
            // callback — the network fetch above is already concurrent
            // (fast), but decoding+uploading up to a page's worth of
            // textures in a single frame is the part that risks a hard
            // crash (confirmed on real hardware 2026-08-19, see
            // autobuild_card.hpp).
            std::vector<brls::Image*> thumbs;
            std::vector<std::function<bool(brls::Image*)>> loaders;
            for (size_t i = 0; i < responses.size() && i < rowImages_.size(); ++i) {
                thumbs.push_back(rowImages_[i]);
                if (!responses[i].success) {
                    loaders.emplace_back();
                    continue;
                }
                const auto& body = responses[i].body;
                loaders.push_back([body](brls::Image* thumb) { return totk::loadJpegFromMem(thumb, body); });
            }
            totk::ui::applyAutobuildIconsStaggered(thumbs, loaders,
                                                    [this, generation]() { return generation == loadGeneration_; });
        });
    });
}

void AutobuildCatalogActivity::openSearch() {
    brls::Application::getImeManager()->openForText(
        [this](std::string text) {
            searchQuery_ = text;
            loadPage(1);
        },
        "Search HyruleWorks builds", "", 48, searchQuery_);
}

void pushAutobuildCatalogActivity() {
    brls::Application::pushActivity(new AutobuildCatalogActivity(), brls::TransitionAnimation::FADE);
}

}  // namespace totk::ui
