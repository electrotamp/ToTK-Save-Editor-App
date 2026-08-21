#include "ui/autobuild_detail_activity.hpp"

#include "app/app_state.hpp"
#include "net/http_client.hpp"
#include "save/autobuilder_cai.hpp"
#include "save/save_editor.hpp"
#include "ui/autobuild_icon_store.hpp"
#include "ui/autobuild_slot_picker.hpp"
#include "ui/zonai_wisp_overlay.hpp"
#include "util/image_loader.hpp"
#include "util/totk_log.hpp"

#include <borealis/core/application.hpp>
#include <borealis/core/box.hpp>
#include <borealis/views/applet_frame.hpp>
#include <borealis/views/image.hpp>
#include <borealis/views/label.hpp>

#include <memory>

namespace totk::ui {

AutobuildDetailActivity::AutobuildDetailActivity(int buildId) : buildId_(buildId) {}

brls::View* AutobuildDetailActivity::createContentView() {
    auto* root = new brls::Box();
    root->setAxis(brls::Axis::COLUMN);
    root->setWidthPercentage(100);
    root->setHeightPercentage(100);
    root->setPadding(16, 16, 16, 16);
    root->setFocusable(false);

    // This screen has no visible controls to focus: its commands live on the
    // activity root. Give Borealis a tiny off-screen descendant as an input
    // focus sink so actions bubble to `root` without drawing a cyan focus
    // rectangle around the entire foreground activity.
    auto* focusSink = new brls::Box();
    focusSink->setPositionType(brls::PositionType::ABSOLUTE);
    focusSink->setPositionLeft(-64);
    focusSink->setPositionTop(-64);
    focusSink->setWidth(1);
    focusSink->setHeight(1);
    focusSink->setFocusable(true);
    focusSink->setHideHighlightBackground(true);
    root->addView(focusSink);
    focusTarget_ = focusSink;

    auto* imageWrap = new brls::Box();
    imageWrap->setWidthPercentage(100);
    imageWrap->setHeight(280);
    imageWrap->setMarginBottom(10);
    imageWrap->setCornerRadius(6);
    imageWrap->setClipsToBounds(true);
    imageWrap->setBackgroundColor(nvgRGBA(16, 18, 20, 128));

    image_ = new brls::Image();
    image_->setWidthPercentage(100);
    image_->setHeightPercentage(100);
    image_->setScalingType(brls::ImageScalingType::FIT);
    // Borealis's clipped FIT path fills the entire view with NanoVG's image
    // paint, which clamps the outermost pixels into long horizontal smears
    // when the source is narrower than this preview box. The FIT-computed
    // image bounds are already centered and aspect-correct; drawing only
    // those bounds leaves clean letterboxing around them instead.
    image_->setClipsToBounds(false);
    image_->setCornerRadius(6);
    imageWrap->addView(image_);
    root->addView(imageWrap);

    auto* detailsPanel = new brls::Box();
    detailsPanel->setAxis(brls::Axis::COLUMN);
    detailsPanel->setWidthPercentage(100);
    detailsPanel->setPadding(10, 12, 12, 12);
    detailsPanel->setCornerRadius(6);
    detailsPanel->setBackgroundColor(nvgRGBA(28, 30, 32, 128));

    imageCounterLabel_ = new brls::Label();
    imageCounterLabel_->setFocusable(false);
    imageCounterLabel_->setFontSize(13);
    imageCounterLabel_->setTextColor(nvgRGB(160, 160, 160));
    imageCounterLabel_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    imageCounterLabel_->setMarginBottom(10);
    detailsPanel->addView(imageCounterLabel_);

    titleLabel_ = new brls::Label();
    titleLabel_->setFocusable(false);
    titleLabel_->setFontSize(24);
    titleLabel_->setText("Loading...");
    detailsPanel->addView(titleLabel_);

    creatorLabel_ = new brls::Label();
    creatorLabel_->setFocusable(false);
    creatorLabel_->setFontSize(15);
    creatorLabel_->setTextColor(nvgRGB(160, 160, 160));
    creatorLabel_->setMarginBottom(8);
    detailsPanel->addView(creatorLabel_);

    descriptionLabel_ = new brls::Label();
    descriptionLabel_->setFocusable(false);
    descriptionLabel_->setFontSize(16);
    // Percentage width under this nested Yoga layout was still measured as
    // unconstrained on hardware, leaving long descriptions on one clipped,
    // animated line. An explicit content width forces Label's multiline
    // measurement and wrapping path.
    descriptionLabel_->setWidth(1200);
    descriptionLabel_->setSingleLine(false);
    detailsPanel->addView(descriptionLabel_);
    root->addView(detailsPanel);

    auto* spacer = new brls::Box();
    spacer->setGrow(1.0f);
    spacer->setFocusable(false);
    root->addView(spacer);

    auto* statusPanel = new brls::Box();
    statusPanel->setAxis(brls::Axis::COLUMN);
    statusPanel->setWidthPercentage(100);
    statusPanel->setPadding(8, 12, 8, 12);
    statusPanel->setCornerRadius(6);
    statusPanel->setBackgroundColor(nvgRGBA(28, 30, 32, 128));

    statusLabel_ = new brls::Label();
    statusLabel_->setFocusable(false);
    statusLabel_->setFontSize(14);
    statusLabel_->setTextColor(nvgRGB(160, 160, 160));
    statusPanel->addView(statusLabel_);
    root->addView(statusPanel);

    root->registerAction(
        "Back", brls::BUTTON_B,
        [](brls::View*) {
            brls::Application::popActivity(brls::TransitionAnimation::FADE);
            return true;
        },
        false, true);
    root->registerAction(
        "Import to Slot", brls::BUTTON_A, [this](brls::View*) { doImport(); return true; }, false, true);
    root->registerAction(
        "Prev Image", brls::BUTTON_LB,
        [this](brls::View*) {
            if (!detail_.imageUrls.empty())
                showImage((currentImageIndex_ + detail_.imageUrls.size() - 1) % detail_.imageUrls.size());
            return true;
        },
        false, true);
    root->registerAction(
        "Next Image", brls::BUTTON_RB,
        [this](brls::View*) {
            if (!detail_.imageUrls.empty()) showImage((currentImageIndex_ + 1) % detail_.imageUrls.size());
            return true;
        },
        false, true);

    auto* frame = new brls::AppletFrame(root);
    totk::ui::setCenteredHeaderTitle(frame, "Autobuild");
    totk::ui::attachEditorBackground(frame);

    loadDetail();
    return frame;
}

void AutobuildDetailActivity::onContentAvailable() {
    brls::sync([this]() {
        if (!focusTarget_) return;
        brls::Application::giveFocus(focusTarget_);
        TOTK_LOG("autobuild detail: foreground focus established");
    });
}

void AutobuildDetailActivity::loadDetail() {
    ++generation_;
    const int generation = generation_;
    if (statusLabel_) statusLabel_->setText("Loading build details...");

    const int buildId = buildId_;
    brls::async([this, buildId, generation]() {
        totk::net::HyruleWorksBuildDetail detail;
        std::string error;
        const bool ok = totk::net::fetchHyruleWorksBuildDetail(buildId, detail, error);
        brls::sync([this, generation, ok, detail, error]() {
            if (generation != generation_) return;
            if (!ok) {
                if (statusLabel_) statusLabel_->setText(error.empty() ? "Failed to load build." : error);
                return;
            }

            detail_ = detail;
            if (titleLabel_) titleLabel_->setText(detail_.name.empty() ? "(untitled)" : detail_.name);
            if (creatorLabel_) {
                creatorLabel_->setText("by " + (detail_.creator.empty() ? std::string("unknown") : detail_.creator));
            }
            if (descriptionLabel_) descriptionLabel_->setText(detail_.description);
            if (statusLabel_) {
                statusLabel_->setText(detail_.caiFileUrl.empty() ? "This build has no downloadable blueprint."
                                                                  : "Press A to import to an Autobuild slot.");
            }
            showImage(0);
        });
    });
}

void AutobuildDetailActivity::showImage(size_t index) {
    if (detail_.imageUrls.empty()) {
        if (imageCounterLabel_) imageCounterLabel_->setText("No preview image.");
        return;
    }

    currentImageIndex_ = index % detail_.imageUrls.size();
    if (imageCounterLabel_) {
        imageCounterLabel_->setText(detail_.imageUrls.size() > 1
                                         ? "Image " + std::to_string(currentImageIndex_ + 1) + " / " +
                                               std::to_string(detail_.imageUrls.size()) + "  (L/R to cycle)"
                                         : "");
    }

    const std::string url = totk::net::hyruleWorksImageCdnUrl(detail_.imageUrls[currentImageIndex_], 640);
    const int generation = generation_;
    const size_t requestIndex = currentImageIndex_;
    brls::async([this, url, generation, requestIndex]() {
        const totk::net::HttpResponse response = totk::net::HttpClient::get(url);
        brls::sync([this, generation, requestIndex, response]() {
            if (generation != generation_ || requestIndex != currentImageIndex_) return;  // superseded
            if (response.success && image_ && !totk::loadJpegFromMem(image_, response.body)) {
                TOTK_LOG("autobuild detail: image decode failed");
            }
        });
    });
}

void AutobuildDetailActivity::doImport() {
    if (detail_.caiFileUrl.empty()) {
        brls::Application::notify("This build has no downloadable blueprint.");
        return;
    }

    const std::string caiUrl = detail_.caiFileUrl;
    if (statusLabel_) statusLabel_->setText("Downloading blueprint...");
    brls::Application::blockInputs(true);

    brls::async([this, caiUrl]() {
        std::vector<uint8_t> bytes;
        std::string error;
        const bool downloaded = totk::net::downloadHyruleWorksBlueprint(caiUrl, bytes, error);

        totk::AutobuilderCai blueprint;
        std::string parseError;
        const bool parsed = downloaded && totk::parseAutobuilderCai(bytes, blueprint, parseError);

        brls::sync([this, downloaded, error, parsed, parseError, blueprint]() {
            brls::Application::unblockInputs();
            if (!downloaded) {
                if (statusLabel_) statusLabel_->setText(error);
                return;
            }
            if (!parsed) {
                if (statusLabel_) statusLabel_->setText(parseError);
                return;
            }

            auto blueprintPtr = std::make_shared<totk::AutobuilderCai>(blueprint);
            const std::string firstImageUrl = detail_.imageUrls.empty() ? detail_.imageUrl : detail_.imageUrls.front();
            pushAutobuildSlotPicker([blueprintPtr, firstImageUrl](totk::ui::AutobuildImportTarget target) {
                auto& editor = AppState::instance().editor();
                std::string importError;
                size_t draftPosition = 0;
                const bool imported = target.isFavorite
                    ? editor.importAutobuilderFavorite(target.favoriteIndex, *blueprintPtr, importError, &draftPosition)
                    : editor.importAutobuilderHistory(target.historyDraftPosition, *blueprintPtr, importError,
                                                       &draftPosition);
                if (imported) {
                    editor.saveAutobuilder();

                    // Deliberately not touching the console's icon cache
                    // here (invalidate or otherwise) — confirmed 2026-08-19
                    // that a full clear is the only thing that visibly
                    // resets an in-game tile, but it wipes every other
                    // Favorite/Weapon/Shield render too, which is worse than
                    // the stale-thumbnail-on-a-reused-position problem it
                    // solves. A freshly imported build on a never-used
                    // position already shows blank in-game with no code
                    // needed. This app's own Autobuild list uses a separate
                    // SD-side icon (below), unaffected by any of this.
                    if (!firstImageUrl.empty()) {
                        const std::string url = totk::net::hyruleWorksImageCdnUrl(firstImageUrl, 300);
                        const auto blueprintBytes = blueprintPtr->combinedActorInfo;
                        brls::async([blueprintBytes, url]() {
                            const totk::net::HttpResponse response = totk::net::HttpClient::get(url);
                            if (response.success) {
                                totk::ui::saveAutobuildIcon(blueprintBytes, response.body);
                            }
                        });
                    }
                }

                brls::Application::popActivity(brls::TransitionAnimation::FADE, [imported, importError, target]() {
                    std::string message;
                    if (imported) {
                        message = target.isFavorite ? "Imported to Favorite " + std::to_string(target.favoriteIndex + 1)
                                                     : "Imported to History";
                    } else {
                        message = importError;
                    }
                    brls::Application::notify(message);
                });
            });
        });
    });
}

void pushAutobuildDetailActivity(int buildId) {
    brls::Application::pushActivity(new AutobuildDetailActivity(buildId), brls::TransitionAnimation::FADE);
}

}  // namespace totk::ui
