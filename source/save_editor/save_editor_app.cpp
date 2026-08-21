#include "save_editor/save_editor_app.hpp"

#include "app/app_state.hpp"
#include "save_editor/glow_panel.hpp"
#include "save_editor/heart_icon.hpp"
#include "save_editor/save_editor_stats.hpp"
#include "save_editor/tab_host_activity.hpp"
#include "save/caption_parser.hpp"
#include "save/completism_data.hpp"
#include "save/save_editor.hpp"
#include "ui/theme_import_activity.hpp"
#include "ui/zonai_wisp_overlay.hpp"
#include "util/debug_stage.hpp"
#include "util/icon_atlas.hpp"
#include "util/image_loader.hpp"
#include "util/perf_trace.hpp"
#include "util/totk_log.hpp"

#include <borealis/core/application.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/core/touch/tap_gesture.hpp>

#include <algorithm>

#if defined(__SWITCH__)
#include "platform/switch_save_mount.hpp"
#endif

namespace totk::save_editor {

namespace {

#if defined(TOTK_ATLAS_ALL)
constexpr const char* kWeaponsSheetPath = "assets/icon_atlas/atlas_0.png";
#else
constexpr const char* kWeaponsSheetPath = "assets/icon_atlas/weapons_0.png";
#endif

// Picker master-detail layout: fixed-width glow panel on the left, the rest
// of the content width on the right for the slot list. Shared between
// createContentView (which sizes the ScrollingFrame) and makeSlotRow (which
// must size each row to match — a percentage width here hits the same Yoga
// stretch-under-ScrollingFrame quirk documented on TabHostActivity's grid
// scrollbar-alignment comment, so every row must get this exact pixel width
// like the pre-redesign rows did).
constexpr float kDetailPanelWidth = 460.0f;
constexpr float kSlotListWidth = 1264.0f - 40.0f - kDetailPanelWidth - 24.0f;

void wireVerticalFocus(const std::vector<brls::View*>& rows) {
    for (size_t i = 0; i < rows.size(); ++i) {
        brls::View* row = rows[i];
        if (i > 0) {
            row->setCustomNavigationRoute(brls::FocusDirection::UP, rows[i - 1]);
        } else {
            row->setCustomNavigationRoute(brls::FocusDirection::UP, row);
        }
        if (i + 1 < rows.size()) {
            row->setCustomNavigationRoute(brls::FocusDirection::DOWN, rows[i + 1]);
        } else {
            row->setCustomNavigationRoute(brls::FocusDirection::DOWN, row);
        }
    }
}

// A "Label ......... value" row for the detail panel's stat list. Returns
// the value label so the caller can update its text per-slot; the caption
// label's text is fixed at construction.
brls::Label* addStatRow(brls::Box* parent, const std::string& caption) {
    auto* row = new brls::Box();
    row->setAxis(brls::Axis::ROW);
    row->setAlignItems(brls::AlignItems::CENTER);
    row->setWidthPercentage(100);
    row->setMarginBottom(6.0f);
    row->setFocusable(false);

    auto* label = new brls::Label();
    label->setFocusable(false);
    label->setFontSize(14);
    label->setTextColor(nvgRGB(160, 170, 168));
    label->setText(caption);
    label->setGrow(1.0f);
    row->addView(label);

    auto* value = new brls::Label();
    value->setFocusable(false);
    value->setFontSize(14);
    value->setHorizontalAlign(brls::HorizontalAlign::RIGHT);
    row->addView(value);

    parent->addView(row);
    return value;
}

std::string formatThousands(uint32_t value) {
    std::string digits = std::to_string(value);
    for (int pos = static_cast<int>(digits.size()) - 3; pos > 0; pos -= 3) {
        digits.insert(static_cast<size_t>(pos), ",");
    }
    return digits;
}

bool preloadWeaponsAtlasSheet() {
    TOTK_STAGE("editor.atlas.ensureLoaded");
    if (!IconAtlas::instance().ensureLoaded()) {
        TOTK_LOG("editor: atlas manifest load failed");
        return false;
    }
    TOTK_STAGE("editor.atlas.sheetResident");
    if (!IconAtlas::instance().ensureSheetResident(kWeaponsSheetPath)) {
        TOTK_LOG("editor: weapons sheet GPU load failed");
        return false;
    }
    TOTK_LOG("editor: weapons sheet ready entries=%zu", IconAtlas::instance().entryCount());
    return true;
}

}  // namespace

brls::View* PickerActivity::createContentView() {
    auto* root = new brls::Box();
    root->setAxis(brls::Axis::ROW);
    root->setWidthPercentage(100);
    root->setHeightPercentage(100);
    root->setGrow(1.0f);
    root->setFocusable(false);
    root->setPadding(12.0f, 40.0f, 28.0f, 40.0f);

    // --- Left: detail panel for the focused slot (thumbnail, location,
    // date, hearts/stamina/playtime) ---
    detailPanel_ = new GlowPanel();
    detailPanel_->setAxis(brls::Axis::COLUMN);
    detailPanel_->setWidth(kDetailPanelWidth);
    detailPanel_->setHeightPercentage(100);
    detailPanel_->setShrink(0);
    detailPanel_->setMarginRight(24.0f);
    detailPanel_->setFocusable(false);
    detailPanel_->setPadding(16.0f);
    // Fully opaque — a translucent panel background let the glow halo bleed
    // through into the thumbnail/text area instead of staying outside the
    // panel edges where it belongs.
    detailPanel_->setBackgroundColor(nvgRGBA(10, 24, 26, 255));

    detailThumb_ = new brls::Image();
    detailThumb_->setFocusable(false);
    detailThumb_->setScalingType(brls::ImageScalingType::FILL);
    detailThumb_->setWidth(kDetailPanelWidth - 32.0f);
    detailThumb_->setHeight((kDetailPanelWidth - 32.0f) * 9.0f / 16.0f);
    detailThumb_->setCornerRadius(8.0f);
    detailThumb_->setMarginBottom(14.0f);
    detailPanel_->addView(detailThumb_);

    auto* slotRow = new brls::Box();
    slotRow->setAxis(brls::Axis::ROW);
    slotRow->setAlignItems(brls::AlignItems::CENTER);
    slotRow->setWidthPercentage(100);
    slotRow->setFocusable(false);

    detailNameLabel_ = new brls::Label();
    detailNameLabel_->setFocusable(false);
    detailNameLabel_->setFontSize(22);
    detailNameLabel_->setGrow(1.0f);
    slotRow->addView(detailNameLabel_);

    detailAutosaveLabel_ = new brls::Label();
    detailAutosaveLabel_->setFocusable(false);
    detailAutosaveLabel_->setFontSize(13);
    detailAutosaveLabel_->setTextColor(nvgRGB(0, 255, 204));
    slotRow->addView(detailAutosaveLabel_);
    detailPanel_->addView(slotRow);

    detailDateLabel_ = new brls::Label();
    detailDateLabel_->setFocusable(false);
    detailDateLabel_->setFontSize(14);
    detailDateLabel_->setTextColor(nvgRGB(160, 170, 168));
    detailDateLabel_->setMarginBottom(14.0f);
    detailPanel_->addView(detailDateLabel_);

    detailPlaytimeLabel_ = new brls::Label();
    detailPlaytimeLabel_->setFocusable(false);
    detailPlaytimeLabel_->setFontSize(16);
    detailPlaytimeLabel_->setTextColor(nvgRGB(200, 205, 204));
    detailPlaytimeLabel_->setMarginBottom(16.0f);
    detailPanel_->addView(detailPlaytimeLabel_);

    detailHeartsRow_ = new brls::Box();
    detailHeartsRow_->setAxis(brls::Axis::ROW);
    detailHeartsRow_->setFocusable(false);
    detailHeartsRow_->setMarginBottom(10.0f);
    detailPanel_->addView(detailHeartsRow_);

    detailStaminaRow_ = new brls::Box();
    detailStaminaRow_->setAxis(brls::Axis::ROW);
    detailStaminaRow_->setFocusable(false);
    detailStaminaRow_->setMarginBottom(18.0f);
    detailPanel_->addView(detailStaminaRow_);

    detailRupeesLabel_ = addStatRow(detailPanel_, "Rupees");
    detailBatteryLabel_ = addStatRow(detailPanel_, "Zonai Charge");
    detailKorokLabel_ = addStatRow(detailPanel_, "Korok Seeds");
    detailShrineLabel_ = addStatRow(detailPanel_, "Shrines");
    detailLightrootLabel_ = addStatRow(detailPanel_, "Lightroots");

    root->addView(detailPanel_);

    // --- Right: compact scrollable slot list ---
    auto* scroll = new brls::ScrollingFrame();
    // root is ROW axis (detail panel + list side by side), so grow(1.0f)
    // here would only compete for horizontal space along the main axis —
    // height needs to be explicit, matching detailPanel_'s
    // setHeightPercentage(100). Without it the frame was sizing to
    // something near-zero, clipping every row's content out of view even
    // though all of them existed in the tree.
    scroll->setHeightPercentage(100);
    scroll->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    scroll->setFocusable(false);
    // Same scrollbar-alignment fix as every other editor screen (explicit
    // pixel width so the frame's right edge — and thus its scrollbar —
    // lands flush with the AppletFrame divider's inset) — see
    // TabHostActivity::buildEquipmentGrid's matching comment. Width is what's left
    // after the 40px left/right page padding and the detail panel.
    scroll->setWidth(kSlotListWidth);
    scroll->setAlignSelf(brls::AlignSelf::FLEX_START);

    slotList_ = new brls::Box();
    slotList_->setAxis(brls::Axis::COLUMN);
    slotList_->setWidthPercentage(100);
    slotList_->setFocusable(false);
    scroll->setContentView(slotList_);
    root->addView(scroll);

    auto* frame = new brls::AppletFrame(root);
    // Left empty — the logo baked into the wallpaper already reads "ToTK
    // Save Editor", and this screen has no tab name to show in its place.
    frame->setTitle("");
    frame->registerAction("Import Theme", brls::BUTTON_X, [](brls::View*) {
        totk::ui::pushThemeImportPicker([](bool success, const std::string& message) {
            brls::Application::notify(message);
        });
        return true;
    });
    // Attached to the AppletFrame itself, not root — see TabHostActivity's
    // matching comment (save_editor_app.cpp -> tab_host_activity.cpp) for why:
    // root is only the frame's content slot, confined between the header and
    // footer bars, so a wallpaper/wisp sized to root fell short of the true
    // screen edges. Also drops the frame's previous ViewBackground::SIDEBAR —
    // that painted its own opaque gradient over the header/footer area,
    // which would have hidden the wallpaper there.
    totk::ui::attachEditorBackground(frame);
    return frame;
}

void PickerActivity::populateSlots() {
    if (slotsPopulated_) return;
    slotsPopulated_ = true;

    if (!slotList_) return;

    AppState::instance().preloadSaveSlots();
    slots_ = AppState::instance().preloadedSaveSlots();
    TOTK_LOG("editor: populateSlots slotList=%p slots=%zu", static_cast<void*>(slotList_), slots_.size());

    if (slots_.empty()) {
        auto* empty = new brls::Label();
        std::string message = "No saves found.";
#if defined(__SWITCH__)
        if (!SwitchSaveMount::isMounted()) {
            message += "\n\n" + SwitchSaveMount::lastError();
        }
        TOTK_LOG("editor: populateSlots empty, mounted=%d error=%s", SwitchSaveMount::isMounted() ? 1 : 0,
                 SwitchSaveMount::lastError().c_str());
#endif
        empty->setText(message);
        slotList_->addView(empty);
        return;
    }

    slotRows_.clear();
    slotRows_.reserve(slots_.size());
    for (size_t i = 0; i < slots_.size(); ++i) {
        brls::Box* row = makeSlotRow(slots_[i], i);
        slotList_->addView(row);
        slotRows_.push_back(row);
    }
    wireVerticalFocus(slotRows_);

    TOTK_LOG("editor: picker ready slots=%zu", slots_.size());

    if (!pendingThumbnails_.empty()) {
        brls::sync([this]() { deferLoadThumbnails(0); });
    }

    if (!focusSubscribed_) {
        focusSubscribed_ = true;
        focusSubscription_ = brls::Application::getGlobalFocusChangeEvent()->subscribe([this](brls::View* view) {
            for (size_t i = 0; i < slotRows_.size(); ++i) {
                if (slotRows_[i] == view) {
                    updateDetailPanel(i);
                    return;
                }
            }
        });
    }

    // Slot 0 computed synchronously (same cost as before — one slot's worth
    // of parsing) since it's shown immediately below; the rest precompute
    // one-per-frame in the background so scrolling to them afterward is
    // instant instead of re-parsing progress.sav on every focus change.
    slotDetails_.assign(slots_.size(), SlotDetail{});
    computeSlotDetail(0);
    updateDetailPanel(0);
    if (slots_.size() > 1) {
        brls::sync([this]() { deferPrecomputeDetails(1); });
    }
}

void PickerActivity::computeSlotDetail(size_t index) {
    if (index >= slots_.size() || index >= slotDetails_.size()) return;
    SlotDetail& detail = slotDetails_[index];
    const auto& slot = slots_[index];

    totk::SaveEditor editor;
    detail.loaded = !slot.progressPath.empty() && editor.loadProgress(slot.progressPath);
    if (!detail.loaded) return;

    const auto& stats = editor.stats();
    detail.playtime = stats.playtime;
    detail.maxHearts = stats.maxHearts;
    detail.maxStamina = stats.maxStamina;
    detail.rupees = stats.rupees;
    detail.maxBattery = stats.maxBattery;

    detail.korokDone = editor.countCompletedHashes("koroks_hidden") + editor.countCompletedHashes("koroks_carry");
    detail.korokTotal = static_cast<int>(totk::CompletismData::instance().hashesFor("koroks_hidden").size() +
                                          totk::CompletismData::instance().hashesFor("koroks_carry").size());
    detail.shrineDone = editor.countCompletedHashes("shrines_found");
    detail.shrineTotal = static_cast<int>(totk::CompletismData::instance().hashesFor("shrines_found").size());
    detail.lightrootDone = editor.countCompletedHashes("lightroots_found");
    detail.lightrootTotal = static_cast<int>(totk::CompletismData::instance().hashesFor("lightroots_found").size());

    TOTK_LOG("editor: slot detail computed slot=%d koroks=%d/%d shrines=%d/%d lightroots=%d/%d", slot.slotIndex,
              detail.korokDone, detail.korokTotal, detail.shrineDone, detail.shrineTotal, detail.lightrootDone,
              detail.lightrootTotal);
}

void PickerActivity::deferPrecomputeDetails(size_t index) {
    if (index >= slots_.size()) return;
    computeSlotDetail(index);
    const size_t next = index + 1;
    if (next < slots_.size()) {
        brls::sync([this, next]() { deferPrecomputeDetails(next); });
    } else {
        TOTK_LOG("editor: slot detail precompute done slots=%zu", slots_.size());
    }
}

void PickerActivity::updateDetailPanel(size_t index) {
    if (index >= slots_.size() || detailLoadedIndex_ == index) return;
    detailLoadedIndex_ = index;
    const auto& slot = slots_[index];

    if (detailNameLabel_) {
        detailNameLabel_->setText(!slot.locationName.empty() ? slot.locationName
                                                               : "Slot " + std::to_string(slot.slotIndex));
    }
    if (detailDateLabel_) {
        detailDateLabel_->setText(slot.hasCaption ? slot.formattedDate : "Save time unavailable");
    }
    if (detailAutosaveLabel_) {
        detailAutosaveLabel_->setText(slot.autosave ? "AUTOSAVE" : "MANUAL SAVE");
    }

    if (detailThumb_) {
        detailThumb_->clear();
        if (slot.hasCaption && !slot.captionPath.empty()) {
            totk::CaptionMetadata caption;
            if (totk::CaptionParser::loadFromFile(slot.captionPath, caption, /*includeThumbnail=*/true)) {
                totk::loadJpegFromMem(detailThumb_, caption.thumbnailJpeg);
            }
        }
    }

    // Hearts/stamina/playtime/completion stats aren't in caption.sav and are
    // too slow to parse fresh on every focus change (full progress.sav parse
    // + hundreds of completion-flag lookups) — read from the precomputed
    // cache instead (see computeSlotDetail/deferPrecomputeDetails). Slot 0 is
    // guaranteed ready by the time this first runs; other slots fill in over
    // the following frames, so a value briefly reads as "--" only if you
    // navigate to a not-yet-computed slot within the first few frames after
    // the picker opens.
    const bool haveDetail = index < slotDetails_.size();
    const SlotDetail emptyDetail{};
    const SlotDetail& detail = haveDetail ? slotDetails_[index] : emptyDetail;
    const bool loaded = detail.loaded;

    if (detailPlaytimeLabel_) {
        detailPlaytimeLabel_->setText(loaded ? detail.playtime : "--:--:--");
    }

    if (detailHeartsRow_) {
        detailHeartsRow_->clearViews();
        // maxHearts is stored in quarter-heart units (see
        // save_editor_stats.cpp's heartOptions()). Fixed 10-heart display is
        // a simplification — real max can go up to 30 hearts — good enough
        // for an at-a-glance preview, not meant to be exact.
        constexpr int kHeartCount = 10;
        const int hearts = loaded ? std::min(kHeartCount, static_cast<int>(detail.maxHearts / 4)) : 0;
        for (int i = 0; i < kHeartCount; ++i) {
            auto* heart = new HeartIcon();
            heart->setWidth(16.0f);
            heart->setHeight(16.0f);
            heart->setMarginRight(3.0f);
            heart->setFilled(i < hearts);
            detailHeartsRow_->addView(heart);
        }
    }

    if (detailStaminaRow_) {
        detailStaminaRow_->clearViews();
        // 3 wheels of 5 fifths each = 15 segments total, matching exactly
        // how the editor's own stamina selector (kStaminaOptions in
        // save_editor_stats.cpp) counts: index 0 ("1 wheel") = 5/15, index 5
        // ("2 wheels") = 10/15, index 10 ("3 wheels") = 15/15 (full). The
        // table's last entry ("Infinite (editor)") isn't a normal wheel step
        // — it clamps to a full bar rather than reading as an extra segment.
        constexpr int kStaminaSegments = 15;
        int filled = 0;
        if (loaded) {
            const int segIndex = staminaWheelSegmentIndex(detail.maxStamina);
            filled = std::min(kStaminaSegments, 5 + segIndex);
        }
        for (int i = 0; i < kStaminaSegments; ++i) {
            auto* seg = new brls::Box();
            seg->setWidth(11.0f);
            seg->setHeight(10.0f);
            seg->setMarginRight(2.0f);
            // Every 5th segment is a wheel boundary — a hair more gap there
            // reads as 3 groups of 5 instead of one undifferentiated strip.
            if ((i + 1) % 5 == 0 && i + 1 != kStaminaSegments) seg->setMarginRight(6.0f);
            seg->setCornerRadius(2.0f);
            seg->setFocusable(false);
            seg->setBackgroundColor(i < filled ? nvgRGB(0, 230, 180) : nvgRGBA(255, 255, 255, 30));
            detailStaminaRow_->addView(seg);
        }
    }

    if (detailRupeesLabel_) {
        detailRupeesLabel_->setText(loaded ? formatThousands(detail.rupees) : "--");
    }
    if (detailBatteryLabel_) {
        // maxBattery is linear (unlike stamina) — 1000 units per energy
        // cell, see batteryOptions() in save_editor_stats.cpp.
        detailBatteryLabel_->setText(
            loaded ? std::to_string(static_cast<int>(detail.maxBattery / 1000.0f)) + " cells" : "--");
    }
    if (detailKorokLabel_) {
        detailKorokLabel_->setText(loaded ? std::to_string(detail.korokDone) + " / " + std::to_string(detail.korokTotal)
                                           : "--");
    }
    if (detailShrineLabel_) {
        detailShrineLabel_->setText(
            loaded ? std::to_string(detail.shrineDone) + " / " + std::to_string(detail.shrineTotal) : "--");
    }
    if (detailLightrootLabel_) {
        detailLightrootLabel_->setText(
            loaded ? std::to_string(detail.lightrootDone) + " / " + std::to_string(detail.lightrootTotal) : "--");
    }
}

brls::Box* PickerActivity::makeSlotRow(const totk::SaveSlotInfo& slot, size_t index) {
    // Compact row: small thumbnail + slot number/date/save-type, sized for
    // the narrower list column next to the detail panel (see
    // createContentView). Location name moved to the detail panel only —
    // not shown per-row anymore.
    constexpr float kRowHeight = 64.0f;
    constexpr float kThumbWidth = 96.0f;

    auto* card = new brls::Box();
    card->setAxis(brls::Axis::ROW);
    // Explicit pixel width, not percentage — see kSlotListWidth's comment.
    card->setWidth(kSlotListWidth);
    card->setAlignSelf(brls::AlignSelf::FLEX_START);
    card->setHeight(kRowHeight);
    card->setAlignItems(brls::AlignItems::CENTER);
    card->setMarginBottom(10.0f);
    card->setFocusable(true);
    card->setCornerRadius(6.0f);
    card->setClipsToBounds(true);
    // Fallback tint if there's no caption thumbnail to show — matches the
    // near-invisible tile tint used everywhere else in the app.
    card->setBackgroundColor(nvgRGBA(255, 255, 255, 12));

    auto* thumbBox = new brls::Box();
    thumbBox->setWidth(kThumbWidth);
    thumbBox->setHeightPercentage(100);
    thumbBox->setShrink(0);
    thumbBox->setFocusable(false);
    thumbBox->setBackgroundColor(nvgRGBA(0, 0, 0, 90));
    card->addView(thumbBox);

    if (slot.hasCaption && !slot.captionPath.empty()) {
        auto* image = new brls::Image();
        image->setFocusable(false);
        image->setScalingType(brls::ImageScalingType::FILL);
        // Explicit pixel size, not percentage — brls::Image installs a custom
        // Yoga measure function (image.cpp's imageMeasureFunc) that recomputes
        // size from the loaded texture once invalidateImageBounds() runs
        // (after the deferred JPEG decode finishes), and that recompute
        // doesn't respect percentage width/height the way a plain Box does.
        image->setWidth(kThumbWidth);
        image->setHeight(kRowHeight);
        thumbBox->addView(image);
        pendingThumbnails_.push_back({image, slot.captionPath});
    }

    auto* textColumn = new brls::Box();
    textColumn->setAxis(brls::Axis::COLUMN);
    textColumn->setGrow(1.0f);
    textColumn->setHeightPercentage(100);
    textColumn->setJustifyContent(brls::JustifyContent::CENTER);
    textColumn->setPadding(4.0f, 12.0f, 4.0f, 12.0f);
    textColumn->setFocusable(false);

    auto* slotLabel = new brls::Label();
    slotLabel->setFocusable(false);
    slotLabel->setText(!slot.locationName.empty() ? slot.locationName : "Slot " + std::to_string(slot.slotIndex));
    slotLabel->setFontSize(15);
    slotLabel->setTextColor(nvgRGB(0, 255, 204));
    textColumn->addView(slotLabel);

    auto* dateLabel = new brls::Label();
    dateLabel->setFocusable(false);
    dateLabel->setText(slot.hasCaption ? slot.formattedDate : "Save time unavailable");
    dateLabel->setFontSize(13);
    dateLabel->setTextColor(nvgRGB(180, 180, 180));
    textColumn->addView(dateLabel);

    auto* saveTypeLabel = new brls::Label();
    saveTypeLabel->setFocusable(false);
    saveTypeLabel->setText(slot.autosave ? "AUTOSAVE" : "MANUAL SAVE");
    saveTypeLabel->setFontSize(12);
    saveTypeLabel->setTextColor(nvgRGB(120, 200, 190));
    textColumn->addView(saveTypeLabel);

    card->addView(textColumn);

    (void)index;

    card->registerAction("Open", brls::BUTTON_A, [slot](brls::View*) {
        const std::string path = slot.progressPath;
        TOTK_LOG("editor: slot pick slot_%d path=%s", slot.slotIndex, path.c_str());
        brls::Application::blockInputs(true);

        // Deferred to the next frame so the load runs at a safe point on the main
        // thread; the async task loop has no exception handling and a small stack.
        brls::sync([path]() {
            TOTK_STAGE("editor.load.begin");
            const bool loaded = AppState::instance().editor().loadProgress(path);
            TOTK_STAGE("editor.load.done ok=%d", loaded ? 1 : 0);

            brls::Application::unblockInputs();

            if (!loaded) {
                brls::Application::notify("Failed to load save");
                TOTK_LOG("editor: load failed %s", path.c_str());
                return;
            }

            AppState::instance().setCurrentSavePath(path);

            if (!preloadWeaponsAtlasSheet()) {
                brls::Application::notify("Atlas not ready");
                return;
            }

            openTabHost("stats");
        });
        return true;
    });
    card->addGestureRecognizer(new brls::TapGestureRecognizer(card));

    return card;
}

void PickerActivity::deferLoadThumbnails(size_t index) {
    if (index >= pendingThumbnails_.size()) return;

    const PendingThumbnail& pending = pendingThumbnails_[index];
    if (pending.image && !pending.captionPath.empty()) {
        totk::CaptionMetadata caption;
        if (totk::CaptionParser::loadFromFile(pending.captionPath, caption, /*includeThumbnail=*/true) &&
            !totk::loadJpegFromMem(pending.image, caption.thumbnailJpeg)) {
            TOTK_LOG("editor: slot thumb decode failed %s", pending.captionPath.c_str());
        }
    }

    // One JPEG decode per frame (via brls::sync recursion), same pacing as the
    // pre-atlas reference's slot thumbnails (source/activity/activities.cpp) —
    // decoding all of them synchronously in one frame would stall the picker
    // on first open.
    const size_t next = index + 1;
    if (next < pendingThumbnails_.size()) {
        brls::sync([this, next]() { deferLoadThumbnails(next); });
    }
}

brls::View* PickerActivity::firstSlotRow() {
    if (!slotList_) return nullptr;
    for (brls::View* child : slotList_->getChildren()) {
        if (child->isFocusable()) return child;
    }
    return nullptr;
}

void PickerActivity::onContentAvailable() {
    populateSlots();
    brls::sync([this]() {
        if (brls::View* focus = firstSlotRow()) {
            brls::Application::giveFocus(focus);
        }
    });
}

void PickerActivity::onResume() {
    TOTK_STAGE("editor.picker.resumed");
    if (!focusSubscribed_ && !slotRows_.empty()) {
        focusSubscribed_ = true;
        focusSubscription_ = brls::Application::getGlobalFocusChangeEvent()->subscribe([this](brls::View* view) {
            for (size_t i = 0; i < slotRows_.size(); ++i) {
                if (slotRows_[i] == view) {
                    updateDetailPanel(i);
                    return;
                }
            }
        });
    }

    // Force a re-read of whatever slot is currently shown: returning here
    // from Stats/Weapons after editing and saving the open slot wouldn't
    // otherwise refresh the detail panel, since updateDetailPanel's cache
    // only refetches on an actual focus change, and the focused row is
    // usually unchanged across a round trip. Recompute that slot's cache
    // entry too, not just re-display it — the cache is what's stale here,
    // not just the on-screen labels.
    //
    // Deferred, and re-checked against the activity stack before running:
    // Borealis's popActivity() (see application.cpp) always calls onResume()
    // on whatever activity sits beneath the one being popped, even when our
    // own L/R tab-switch handler (tab_order.cpp) immediately pushes the next
    // tab's activity right back on top in the same synchronous callback. That
    // makes the picker "resume" for a single invisible instant on *every* tab
    // switch, not just genuine returns to the picker screen — and without
    // this guard, each of those phantom resumes paid for a full progress.sav
    // re-parse plus a full completion-hash rescan, which showed up as a
    // ~300-500ms stall on every L/R press. Waiting a frame and checking
    // whether the picker is still on top lets a real return (Back button,
    // nothing pushed after) refresh normally, while a mid-switch flash skips
    // the recompute entirely.
    if (detailLoadedIndex_ != static_cast<size_t>(-1)) {
        const size_t shown = detailLoadedIndex_;
        brls::sync([this, shown]() {
            const auto stack = brls::Application::getActivitiesStack();
            if (stack.empty() || stack.back() != this) return;
            detailLoadedIndex_ = static_cast<size_t>(-1);
            computeSlotDetail(shown);
            updateDetailPanel(shown);
        });
    }
}

void PickerActivity::willDisappear(bool resetState) {
    // Unsubscribe before any teardown starts — see TabHostActivity's
    // matching comment (destroying a focused row can re-fire this same
    // global event mid-teardown otherwise).
    if (focusSubscribed_) {
        brls::Application::getGlobalFocusChangeEvent()->unsubscribe(focusSubscription_);
        focusSubscribed_ = false;
    }
    brls::Activity::willDisappear(resetState);
}

PickerActivity::~PickerActivity() {
    if (focusSubscribed_) {
        brls::Application::getGlobalFocusChangeEvent()->unsubscribe(focusSubscription_);
        focusSubscribed_ = false;
    }
}

}  // namespace totk::save_editor
