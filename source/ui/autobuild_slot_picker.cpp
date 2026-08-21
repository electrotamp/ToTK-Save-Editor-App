#include "ui/autobuild_slot_picker.hpp"

#include "app/app_state.hpp"
#include "platform/switch_save_mount.hpp"
#include "save/save_editor.hpp"
#include "ui/autobuild_card.hpp"
#include "ui/autobuild_icon_store.hpp"
#include "ui/zonai_wisp_overlay.hpp"
#include "util/image_loader.hpp"

#include <borealis/core/application.hpp>
#include <borealis/core/box.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/applet_frame.hpp>
#include <borealis/views/header.hpp>
#include <borealis/views/label.hpp>
#include <borealis/views/scrolling_frame.hpp>

#include <memory>
#include <unordered_map>

namespace totk::ui {

namespace {

constexpr int kGridColumns = 3;

// Cards live two levels deep (listBox -> row Box -> card Box), unlike the
// old flat row list, so finding the first focusable view for initial focus
// needs a real recursive search rather than a direct-children scan.
brls::View* findFirstFocusable(brls::View* view) {
    if (!view) return nullptr;
    if (view->isFocusable()) return view;
    if (auto* box = dynamic_cast<brls::Box*>(view)) {
        for (brls::View* child : box->getChildren()) {
            if (brls::View* found = findFirstFocusable(child)) return found;
        }
    }
    return nullptr;
}

class AutobuildSlotPickerActivity : public brls::Activity {
public:
    explicit AutobuildSlotPickerActivity(std::function<void(AutobuildImportTarget)> onPicked)
        : onPicked_(std::move(onPicked)) {}

    ~AutobuildSlotPickerActivity() override { *aliveFlag_ = false; }

    brls::View* createContentView() override {
        auto* root = new brls::Box();
        root->setAxis(brls::Axis::COLUMN);
        root->setWidthPercentage(100);
        root->setHeightPercentage(100);
        root->setPadding(16, 16, 16, 16);

        auto* listFrame = new brls::ScrollingFrame();
        listFrame->setGrow(1.0f);
        listFrame->setWidthPercentage(100);
        listFrame->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
        listFrame->setFocusable(false);

        auto* listBox = new brls::Box();
        listBox->setAxis(brls::Axis::COLUMN);
        listBox->setWidthPercentage(100);
        listFrame->setContentView(listBox);
        root->addView(listFrame);

        auto& editor = AppState::instance().editor();
        const auto favoriteSlots = editor.autobuilderFavoriteSlots();
        const auto& allSlots = editor.autobuilderSlots();

        // Same batched-mount pattern as TabHostActivity::buildAutobuildTab.
        // The cache reader resolves each slotValue/Index against the newest
        // lifetime Draft filename; physical array position is only used to
        // attach the result to the right card.
        std::vector<size_t> iconKeys;
        std::vector<size_t> iconDraftPositions;
        for (const auto& slot : favoriteSlots) {
            if (!slot.occupied || slot.draftPosition >= allSlots.size()) continue;
            const int sv = allSlots[slot.draftPosition].slotValue;
            if (sv < 0) continue;
            iconKeys.push_back(static_cast<size_t>(sv));
            iconDraftPositions.push_back(slot.draftPosition);
        }
        for (const auto& slot : allSlots) {
            if (slot.favorite || slot.blueprint.empty() || slot.slotValue < 0) continue;
            iconKeys.push_back(static_cast<size_t>(slot.slotValue));
            iconDraftPositions.push_back(static_cast<size_t>(slot.index));
        }
        auto iconBytes = std::make_shared<std::vector<std::vector<uint8_t>>>();
        totk::SwitchSaveMount::readAutobuildDraftIcons(iconKeys, *iconBytes);
        auto positionToIconIndex = std::make_shared<std::unordered_map<size_t, size_t>>();
        for (size_t i = 0; i < iconDraftPositions.size(); ++i) (*positionToIconIndex)[iconDraftPositions[i]] = i;

        // Icons are populated after both grids exist, staggered across
        // frames (see applyAutobuildIconsStaggered) rather than decoded
        // synchronously while building the cards — confirmed on real
        // hardware 2026-08-19 that doing this many in one frame risks a hard
        // crash. iconBytes/positionToIconIndex are shared_ptrs (not
        // per-closure copies, not references) because these loaders run
        // later, on future frames, after this function has already
        // returned.
        auto loadIconFor = [iconBytes, positionToIconIndex](size_t draftPosition,
                                                            const std::vector<uint8_t>& blueprint) {
            return [iconBytes, positionToIconIndex, draftPosition, blueprint](brls::Image* thumb) {
                const auto savedIcon = totk::ui::loadAutobuildIcon(blueprint);
                if (!savedIcon.empty() && totk::loadJpegFromMem(thumb, savedIcon)) return true;
                const auto it = positionToIconIndex->find(draftPosition);
                if (it != positionToIconIndex->end()) return totk::loadPngFromMem(thumb, (*iconBytes)[it->second]);
                return false;
            };
        };

        std::vector<brls::Image*> pendingThumbs;
        std::vector<std::function<bool(brls::Image*)>> pendingLoaders;

        int occupiedFavorites = 0;
        for (const auto& slot : favoriteSlots) {
            if (slot.occupied) ++occupiedFavorites;
        }

        auto* favoritesHeader = new brls::Header();
        favoritesHeader->setTitle("Favorites (" + std::to_string(occupiedFavorites) + "/" +
                                   std::to_string(totk::SaveEditor::kMaxAutobuildFavorites) + ")");
        listBox->addView(favoritesHeader);

        std::vector<AutobuildCardSpec> favoriteCards;
        for (const auto& slot : favoriteSlots) {
            AutobuildCardSpec spec;
            spec.title = "Favorite " + std::to_string(slot.favoriteIndex + 1);
            spec.statusText = slot.occupied ? "Build present — will be overwritten" : "Empty";
            spec.statusColor = slot.occupied ? nvgRGB(120, 200, 120) : nvgRGB(140, 140, 140);

            const int favoriteIndex = slot.favoriteIndex;
            auto onPicked = onPicked_;
            spec.onSelect = [favoriteIndex, onPicked]() {
                AutobuildImportTarget target;
                target.isFavorite = true;
                target.favoriteIndex = favoriteIndex;
                brls::Application::popActivity(brls::TransitionAnimation::FADE, [target, onPicked]() {
                    if (onPicked) onPicked(target);
                });
            };
            favoriteCards.push_back(std::move(spec));
        }
        std::vector<brls::Image*> favoriteThumbs;
        buildAutobuildCardGrid(listBox, kGridColumns, favoriteCards, &favoriteThumbs);
        for (size_t i = 0; i < favoriteSlots.size(); ++i) {
            pendingThumbs.push_back(favoriteThumbs[i]);
            pendingLoaders.push_back(favoriteSlots[i].occupied
                                         ? loadIconFor(favoriteSlots[i].draftPosition,
                                                       allSlots[favoriteSlots[i].draftPosition].blueprint)
                                                                : std::function<bool(brls::Image*)>{});
        }

        auto* historyHeader = new brls::Header();
        historyHeader->setTitle("History");
        historyHeader->setMarginTop(14);
        listBox->addView(historyHeader);

        std::vector<AutobuildCardSpec> historyCards;
        AutobuildCardSpec newestHistory;
        newestHistory.title = "New History Entry";
        newestHistory.statusText = "Adds newest; replaces oldest unfavorited entry";
        newestHistory.statusColor = nvgRGB(150, 160, 210);
        auto onPicked = onPicked_;
        newestHistory.onSelect = [onPicked]() {
            AutobuildImportTarget target;
            target.isFavorite = false;
            target.historyDraftPosition = -1;
            brls::Application::popActivity(brls::TransitionAnimation::FADE, [target, onPicked]() {
                if (onPicked) onPicked(target);
            });
        };
        historyCards.push_back(std::move(newestHistory));
        buildAutobuildCardGrid(listBox, kGridColumns, historyCards, nullptr);

        applyAutobuildIconsStaggered(pendingThumbs, pendingLoaders,
                                      [aliveFlag = aliveFlag_]() { return *aliveFlag; });

        root->registerAction(
            "Cancel", brls::BUTTON_B,
            [](brls::View*) {
                brls::Application::popActivity(brls::TransitionAnimation::FADE);
                return true;
            },
            false, true);

        auto* frame = new brls::AppletFrame(root);
        totk::ui::setCenteredHeaderTitle(frame, "Choose Autobuild Slot");
        totk::ui::attachEditorBackground(frame);

        brls::View* firstFocusable = findFirstFocusable(listBox);
        if (firstFocusable) {
            brls::sync([firstFocusable]() { brls::Application::giveFocus(firstFocusable); });
        }
        return frame;
    }

private:
    std::function<void(AutobuildImportTarget)> onPicked_;
    // Flips false in the destructor so a still-pending staggered icon-apply
    // batch (see applyAutobuildIconsStaggered) stops instead of touching
    // Image views that no longer exist if the user picks a card or cancels
    // before every icon has loaded.
    std::shared_ptr<bool> aliveFlag_ = std::make_shared<bool>(true);
};

}  // namespace

void pushAutobuildSlotPicker(std::function<void(AutobuildImportTarget)> onPicked) {
    brls::Application::pushActivity(new AutobuildSlotPickerActivity(std::move(onPicked)), brls::TransitionAnimation::FADE);
}

}  // namespace totk::ui
