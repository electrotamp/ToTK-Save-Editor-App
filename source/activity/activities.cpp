#include "activity/activities.hpp"

#include "app/app_state.hpp"
#include "save/save_editor.hpp"
#include "save/caption_parser.hpp"
#include "ui/editor_pager_frame.hpp"
#include "ui/focus_helpers.hpp"
#include "ui/item_database.hpp"
#include "ui/item_list_tab.hpp"
#include "ui/status_tab.hpp"
#include "ui/zonai_wisp_session.hpp"
#include "util/icon_texture_cache.hpp"
#ifdef TOTK_ICON_ATLAS
#include "util/icon_atlas.hpp"
#endif
#include "util/image_loader.hpp"
#include "util/debug_stage.hpp"
#include "util/totk_log.hpp"
#include "util/perf_trace.hpp"

#include <borealis/core/application.hpp>
#include <borealis/core/thread.hpp>
#include <functional>
#include <unordered_set>
#include <vector>
#if defined(__SWITCH__)
#include "platform/switch_save_mount.hpp"
#endif

namespace totk::ui {

void openEditor();

namespace {

std::vector<std::string> collectInventoryIconPaths() {
    auto& editor = AppState::instance().editor();
    auto& db = ItemDatabase::instance();
    auto& cache = IconTextureCache::instance();
    std::unordered_set<std::string> paths;

    auto addPaths = [&](const std::string& category, const std::string& id, uint32_t dyeColor = 0,
                        uint32_t effect = 0) {
        if (id.empty()) return;
        for (const auto& path : db.iconPathCandidates(category, id, dyeColor, effect)) {
            if (cache.hasAsset(path)) {
                paths.insert(path);
            }
        }
    };

    for (const auto& [category, items] : editor.equipment()) {
        for (const auto& item : items) {
            addPaths(category, item.id);
            if (!item.fuseId.empty()) {
                for (const auto& path : db.iconPathCandidatesForId(item.fuseId)) {
                    if (cache.hasAsset(path)) {
                        paths.insert(path);
                    }
                }
            }
        }
    }

    for (const auto& item : editor.armors()) {
        addPaths("armors", item.id, item.dyeColor);
    }

    for (const auto& item : editor.horses()) {
        addPaths("horses", item.id);
    }

    for (const auto& [category, items] : editor.stackItems()) {
        for (const auto& item : items) {
            addPaths(category, item.id, 0, item.effect);
        }
    }

    return std::vector<std::string>(paths.begin(), paths.end());
}

void warmInventoryIconCacheBatched(std::function<void()> onComplete) {
    TOTK_PERF("startup.warmIconCache begin");
    IconTextureCache::instance().warmPathsBatched(collectInventoryIconPaths(), std::move(onComplete));
}

struct PendingSlotThumbnail {
    brls::Image* image = nullptr;
    std::string captionPath;
};

std::vector<PendingSlotThumbnail> pendingSlotThumbnails;

void deferLoadSlotThumbnails(size_t index) {
    if (index >= pendingSlotThumbnails.size()) return;

    TOTK_STAGE("slot.thumb.begin i=%zu/%zu", index, pendingSlotThumbnails.size());
    const PendingSlotThumbnail& pending = pendingSlotThumbnails[index];
    if (pending.image && !pending.captionPath.empty()) {
        totk::CaptionMetadata caption;
        if (totk::CaptionParser::loadFromFile(pending.captionPath, caption, true) &&
            !totk::loadJpegFromMem(pending.image, caption.thumbnailJpeg)) {
            TOTK_LOG("slot thumb: decode failed %s", pending.captionPath.c_str());
            TOTK_STAGE("slot.thumb.decode_fail path=%s", pending.captionPath.c_str());
        } else {
            TOTK_STAGE("slot.thumb.ok path=%s bytes=%zu", pending.captionPath.c_str(),
                       caption.thumbnailJpeg.size());
        }
    }

    const size_t next = index + 1;
    if (next < pendingSlotThumbnails.size()) {
        brls::sync([next]() { deferLoadSlotThumbnails(next); });
    } else {
        TOTK_STAGE("slot.thumb.all_done count=%zu", pendingSlotThumbnails.size());
    }
}

void startDeferredSlotThumbnails() {
#if defined(TOTK_DEBUG_NO_SLOT_THUMBS) && TOTK_DEBUG_NO_SLOT_THUMBS
    TOTK_STAGE("slot.thumb.skipped compile_flag=1 count=%zu", pendingSlotThumbnails.size());
    pendingSlotThumbnails.clear();
    return;
#endif
    if (pendingSlotThumbnails.empty()) {
        TOTK_STAGE("slot.thumb.none");
        return;
    }
    TOTK_STAGE("slot.thumb.queue count=%zu", pendingSlotThumbnails.size());
    brls::sync([]() { deferLoadSlotThumbnails(0); });
}

brls::Box* makeSlotRow(const totk::SaveSlotInfo& slot) {
    auto* row = new brls::Box();
    row->setAxis(brls::Axis::ROW);
    row->setWidthPercentage(100);
    row->setMarginBottom(12);
    row->setFocusable(true);

#if !defined(TOTK_DEBUG_NO_SLOT_THUMBS) || !TOTK_DEBUG_NO_SLOT_THUMBS
    if (slot.hasCaption && !slot.captionPath.empty()) {
        auto* image = new brls::Image();
        image->setFocusable(false);
        image->setScalingType(brls::ImageScalingType::FIT);
        image->setWidth(192);
        image->setHeight(108);
        image->setMarginRight(12);
        row->addView(image);
        pendingSlotThumbnails.push_back(PendingSlotThumbnail{image, slot.captionPath});
    }
#endif

    auto* column = new brls::Box();
    column->setAxis(brls::Axis::COLUMN);
    column->setGrow(1.0f);
    column->setFocusable(false);

    auto* dateLabel = new brls::Label();
    dateLabel->setFocusable(false);
    dateLabel->setText(slot.hasCaption ? slot.formattedDate : "Save time unavailable");
    dateLabel->setFontSize(18);
    column->addView(dateLabel);

    auto* metaLabel = new brls::Label();
    metaLabel->setFocusable(false);
    std::string meta = "slot_0" + std::to_string(slot.slotIndex);
    meta += slot.autosave ? " · Autosave" : " · Manual save";
    metaLabel->setText(meta);
    metaLabel->setFontSize(16);
    metaLabel->setTextColor(nvgRGB(160, 160, 160));
    column->addView(metaLabel);

    row->addView(column);

    auto* openLabel = new brls::Label();
    openLabel->setFocusable(false);
    openLabel->setText("Open");
    openLabel->setFontSize(16);
    openLabel->setMarginLeft(12);
    row->addView(openLabel);

    row->registerAction("Open", brls::BUTTON_A, [slot](brls::View*) {
        const std::string path = slot.progressPath;
        const int slotIndex = slot.slotIndex;
        TOTK_LOG("slot pick: slot_%d path=%s (async load)", slotIndex, path.c_str());
        brls::Application::blockInputs(true);

        brls::async([path]() {
            TOTK_PERF_SCOPE("save.loadProgress");
            const uint64_t startMs = totk::perfNowMs();
            const bool loaded = AppState::instance().editor().loadProgress(path);
            TOTK_PERF("save.loadProgress done ok=%d ms=%llu path=%s", loaded ? 1 : 0,
                      static_cast<unsigned long long>(totk::perfNowMs() - startMs), path.c_str());
            brls::sync([path, loaded]() {
                brls::Application::unblockInputs();
                if (!loaded) {
                    TOTK_LOG("loadProgress failed for %s", path.c_str());
                    brls::Application::notify("Failed to load save");
                    return;
                }

                AppState::instance().setCurrentSavePath(path);
                brls::Application::blockInputs(true);
#ifdef TOTK_ICON_ATLAS
                totk::IconAtlas::instance().ensureLoaded();
                TOTK_STAGE("editor.open atlas_ready");
#endif
#if !defined(TOTK_WEAPONS_ONLY_UI)
                totk::ui::ZonaiWispSession::instance().ensureRunning();
#endif
#if !defined(TOTK_ICON_ATLAS)
                warmInventoryIconCacheBatched([]() {
                    brls::Application::unblockInputs();
                    openEditor();
                });
#else
                brls::Application::unblockInputs();
                openEditor();
#endif
            });
        });
        return true;
    });

    return row;
}

EditorPagerFrame* findEditorPager(brls::AppletFrame* frame) {
    if (!frame) return nullptr;
    for (brls::View* child : frame->getChildren()) {
        if (auto* pager = dynamic_cast<EditorPagerFrame*>(child)) {
            return pager;
        }
    }
    return nullptr;
}

void ensureSaveSlotsLoaded() {
    TOTK_STAGE("slot.ensure.begin");
    auto& state = AppState::instance();
    if (state.hasPreloadedSaveSlots()) {
        TOTK_STAGE("slot.ensure.skip cached");
        return;
    }

#if defined(__SWITCH__)
    TOTK_STAGE("slot.ensure.mount.begin");
    if (!SwitchSaveMount::isMounted()) {
        SwitchSaveMount::initialize();
        if (SwitchSaveMount::isMounted()) {
            state.setSaveRoot(SwitchSaveMount::saveRoot());
            TOTK_STAGE("slot.ensure.mount ok root=%s", SwitchSaveMount::saveRoot().c_str());
        } else {
            TOTK_STAGE("slot.ensure.mount fail err=%s", SwitchSaveMount::lastError().c_str());
        }
    } else {
        TOTK_STAGE("slot.ensure.mount already");
    }
#endif
    TOTK_STAGE("slot.ensure.scan.begin");
    state.preloadSaveSlots();
    TOTK_STAGE("slot.ensure.scan.done slots=%zu", state.preloadedSaveSlots().size());
}

void populateSaveSlotList(brls::Box* list) {
    if (!list) return;

    TOTK_STAGE("slot.list.clear");
    pendingSlotThumbnails.clear();
    ensureSaveSlotsLoaded();
    const auto& slots = AppState::instance().preloadedSaveSlots();
    TOTK_STAGE("slot.list.slots=%zu", slots.size());
    if (slots.empty()) {
        auto* empty = new brls::Label();
        std::string message = "No saves found.";
#if defined(__SWITCH__)
        if (!SwitchSaveMount::isMounted()) {
            message += "\n\nCould not mount TotK save data:\n" + SwitchSaveMount::lastError();
            message += "\n\nMake sure the game is installed and you have a save on the active user profile.";
        } else {
            message += "\n\nNo slot_00..slot_05 folders were found in your TotK save.";
        }
#else
        message += "\nPlace slot folders under:\n" + AppState::instance().saveRoot();
#endif
        empty->setText(message);
        list->addView(empty);
        return;
    }

    TOTK_STAGE("slot.list.header.new");
    // Plain label avoids brls::Header XML inflation (was hard-crashing on Switch here).
    auto* header = new brls::Label();
    header->setFocusable(false);
    header->setText("Select Save Slot");
    header->setFontSize(18);
    header->setMarginBottom(12);
    list->addView(header);
    TOTK_STAGE("slot.list.header");

    std::vector<brls::View*> slotRows;
    slotRows.reserve(slots.size());
    for (size_t i = 0; i < slots.size(); ++i) {
        TOTK_STAGE("slot.list.row begin i=%zu slot=%d", i, slots[i].slotIndex);
        brls::View* row = makeSlotRow(slots[i]);
        list->addView(row);
        slotRows.push_back(row);
        TOTK_STAGE("slot.list.row done i=%zu", i);
    }
    for (size_t i = 0; i < slotRows.size(); ++i) {
        brls::View* row = slotRows[i];
        if (i > 0) {
            row->setCustomNavigationRoute(brls::FocusDirection::UP, slotRows[i - 1]);
        } else {
            row->setCustomNavigationRoute(brls::FocusDirection::UP, row);
        }
        if (i + 1 < slotRows.size()) {
            row->setCustomNavigationRoute(brls::FocusDirection::DOWN, slotRows[i + 1]);
        } else {
            row->setCustomNavigationRoute(brls::FocusDirection::DOWN, row);
        }
    }
    TOTK_STAGE("slot.list.complete rows=%zu", list->getChildren().size());
}

}  // namespace

void BootSplashActivity::onContentAvailable() {
    if (auto* frame = dynamic_cast<brls::AppletFrame*>(this->getContentView())) {
        frame->setBackgroundColor(nvgRGB(0, 0, 0));
    }

    AppState::instance().preloadSaveSlots();

    if (auto* splash = dynamic_cast<brls::Box*>(this->getView("splashRoot"))) {
        splash->setBackgroundColor(nvgRGB(0, 0, 0));
        splash->setFocusable(false);
    }

    focusSink_ = new brls::Box();
    focusSink_->setPositionType(brls::PositionType::ABSOLUTE);
    focusSink_->setPositionTop(0);
    focusSink_->setPositionLeft(0);
    focusSink_->setWidth(1);
    focusSink_->setHeight(1);
    focusSink_->setAlpha(0.0f);
    focusSink_->setFocusable(true);
    focusSink_->setHideHighlightBackground(true);
    focusSink_->setCustomNavigationRoute(brls::FocusDirection::UP, focusSink_);
    focusSink_->setCustomNavigationRoute(brls::FocusDirection::DOWN, focusSink_);
    focusSink_->setCustomNavigationRoute(brls::FocusDirection::LEFT, focusSink_);
    focusSink_->setCustomNavigationRoute(brls::FocusDirection::RIGHT, focusSink_);
    if (auto* frame = dynamic_cast<brls::AppletFrame*>(this->getContentView())) {
        frame->addView(focusSink_);
    }

    brls::Application::giveFocus(focusSink_);

    TOTK_LOG("boot splash: showing for %lld ms", static_cast<long long>(kDisplayDurationMs));

    dismissTimer_.setEndCallback([this](bool finished) {
        if (!finished) return;
        finishDismiss();
    });
    dismissTimer_.start(kDisplayDurationMs);
}

void BootSplashActivity::finishDismiss() {
    TOTK_STAGE("boot.splash.finished");

    if (focusSink_) {
        focusSink_->setFocusable(false);
    }

    const auto stack = brls::Application::getActivitiesStack();
    TOTK_STAGE("boot.dismiss stack=%zu", stack.size());

    SlotPickerActivity* picker = nullptr;
    if (stack.size() >= 2) {
        picker = dynamic_cast<SlotPickerActivity*>(stack[stack.size() - 2]);
    }

    if (picker) {
        // Move focus off splash before pop; populate while splash is still stacked (stable order).
        if (brls::View* content = picker->getContentView()) {
            brls::View* target = content->getDefaultFocus();
            if (!target) target = content;
            brls::Application::giveFocus(target);
        }
        TOTK_STAGE("boot.dismiss.build.begin");
        picker->prepareSlots();
        TOTK_STAGE("boot.dismiss.build.done");
        startDeferredSlotThumbnails();
        if (brls::View* focus = picker->firstFocusTarget()) {
            TOTK_STAGE("boot.dismiss.focus target=%s", focus::viewLabel(focus).c_str());
            brls::Application::giveFocus(focus);
        }
    } else {
        TOTK_STAGE("boot.dismiss.build abort not_picker");
    }

    if (focusSink_) {
        focusSink_->setFocusable(false);
        if (auto* parent = dynamic_cast<brls::Box*>(focusSink_->getParent())) {
            parent->removeView(focusSink_);
        }
        focusSink_ = nullptr;
    }

    TOTK_STAGE("boot.dismiss.pop.begin");
    const bool popped = brls::Application::popActivity(brls::TransitionAnimation::NONE);
    TOTK_STAGE("boot.dismiss.pop ok=%d stack=%zu", popped ? 1 : 0,
               brls::Application::getActivitiesStack().size());
    if (!popped) {
        TOTK_LOG("boot splash: popActivity failed");
    }

    TOTK_STAGE("boot.dismiss.ready");
}

void BootSplashActivity::willDisappear(bool resetState) {
    TOTK_STAGE("boot.splash.willDisappear reset=%d", resetState ? 1 : 0);
    brls::Activity::willDisappear(resetState);
    if (dismissTimer_.isRunning()) {
        dismissTimer_.stop();
    }
    if (focusSink_) {
        focusSink_->setFocusable(false);
    }
}

void openEditor() {
    TOTK_PERF_SCOPE("editor.open");
    TOTK_STAGE("editor.open.begin");
#ifdef TOTK_WEAPONS_ONLY_UI
    TOTK_LOG("openWeaponsEditor");
    brls::Application::pushActivity(new WeaponsEditorActivity());
#else
    TOTK_LOG("openEditor");
    brls::Application::pushActivity(new EditorActivity());
    auto stack = brls::Application::getActivitiesStack();
    if (!stack.empty()) {
        if (auto* editor = dynamic_cast<EditorActivity*>(stack.back())) {
            editor->scheduleHomeFocus();
        }
    }
#endif
    TOTK_STAGE("editor.open.done");
}

brls::View* SlotPickerActivity::firstFocusTarget() {
    if (auto* list = dynamic_cast<brls::Box*>(this->getView("slotList"))) {
        for (brls::View* child : list->getChildren()) {
            if (!child->isFocusable()) continue;
            if (brls::View* focus = child->getDefaultFocus()) {
                return focus;
            }
            return child;
        }
    }
    return getContentView() ? getContentView()->getDefaultFocus() : nullptr;
}

void SlotPickerActivity::onContentAvailable() {
    TOTK_STAGE("picker.content.available");
    prepareSlots();
    brls::sync([this]() {
        if (brls::View* focus = firstFocusTarget()) {
            TOTK_STAGE("picker.content.focus target=%s", focus::viewLabel(focus).c_str());
            focus::scheduleVisibleFocus(focus);
        }
    });
}

void SlotPickerActivity::prepareSlots() {
    if (slotsPopulated_) {
        TOTK_STAGE("prepareSlots.skip already_populated");
        return;
    }
    TOTK_STAGE("prepareSlots.begin");

    if (auto* frame = dynamic_cast<brls::AppletFrame*>(this->getContentView())) {
        frame->setBackground(brls::ViewBackground::SIDEBAR);
        frame->setTitle("ToTK Save Editor");
    }

    auto* list = dynamic_cast<brls::Box*>(this->getView("slotList"));
    if (!list) {
        TOTK_LOG("prepareSlots: slotList missing");
        TOTK_STAGE("prepareSlots.abort missing_list");
        return;
    }

    slotsPopulated_ = true;
    populateSaveSlotList(list);
    TOTK_LOG("prepareSlots done rows=%zu thumbs_pending=%zu", list->getChildren().size(),
             pendingSlotThumbnails.size());
    TOTK_STAGE("prepareSlots.done rows=%zu thumbs=%zu", list->getChildren().size(),
               pendingSlotThumbnails.size());
}

void SlotPickerActivity::onResume() {
    brls::Activity::onResume();
    const auto stack = brls::Application::getActivitiesStack();
    if (stack.empty() || stack.back() != this) {
        return;
    }
    if (!slotsPopulated_) {
        return;
    }

    brls::View* current = brls::Application::getCurrentFocus();
    if (current && focus::focusBelongsTo(current, getContentView())) {
        return;
    }

    if (brls::View* focus = firstFocusTarget()) {
        focus::scheduleVisibleFocus(focus);
    }
}

void EditorActivity::scheduleHomeFocus() {
    auto* frame = dynamic_cast<brls::AppletFrame*>(this->getContentView());
    if (!frame) return;

    brls::sync([frame]() {
        if (auto* pager = findEditorPager(frame)) {
            pager->refreshChrome();
            pager->focusActivePage();
        }
    });
}

void EditorActivity::willAppear(bool resetState) {
    brls::Activity::willAppear(resetState);
    scheduleHomeFocus();
}

void EditorActivity::onContentAvailable() {
    auto* frame = dynamic_cast<brls::AppletFrame*>(this->getContentView());
    if (frame) {
        if (auto* pager = findEditorPager(frame)) {
            brls::sync([pager]() { pager->refreshChrome(); });
        }

        frame->registerAction("Save", brls::BUTTON_Y, [this](brls::View*) {
            TOTK_LOG("save requested (Y)");
            if (auto* status = dynamic_cast<StatusTab*>(this->getView("statusTab"))) {
                status->apply();
            }
            auto& state = AppState::instance();
            if (state.editor().saveProgress(state.currentSavePath())) {
                brls::Application::notify("Save written successfully");
            } else {
                brls::Application::notify("Failed to save");
            }
            return true;
        });
    }

    scheduleHomeFocus();
}

void WeaponsEditorActivity::onContentAvailable() {
    if (auto* frame = dynamic_cast<brls::AppletFrame*>(this->getContentView())) {
        frame->setTitle("ToTK Save Editor · Weapons");
        frame->registerAction("Back", brls::BUTTON_B, [](brls::View*) {
            brls::Application::popActivity(brls::TransitionAnimation::NONE);
            return true;
        });
    }

    brls::sync([this]() {
        if (auto* tab = dynamic_cast<ItemListTab*>(this->getView("weaponsTab"))) {
            tab->refresh();
        }
    });
}

void WeaponsEditorActivity::willAppear(bool resetState) {
    brls::Activity::willAppear(resetState);
    brls::sync([this]() {
        if (auto* tab = dynamic_cast<ItemListTab*>(this->getView("weaponsTab"))) {
            tab->activatePageFocus();
        }
    });
}

}  // namespace totk::ui
