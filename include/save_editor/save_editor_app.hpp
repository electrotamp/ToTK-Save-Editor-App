#pragma once

#include <borealis.hpp>

#include <string>
#include <vector>

namespace totk {
struct SaveSlotInfo;
}

namespace totk::save_editor {

class GlowPanel;

// Master-detail picker: a compact scrollable list of save slots on the
// right, and a larger preview panel on the left showing the focused slot's
// thumbnail, location, date, and hearts/stamina/playtime/completion stats.
// The latter aren't in caption.sav — they're read from each slot's own
// progress.sav via a throwaway SaveEditor, independent of AppState's
// currently-opened save. That read (full parse + hash-table index +
// hundreds of completion-flag lookups for koroks/shrines/lightroots) is too
// slow to redo on every focus change, so it's precomputed for every slot
// once, spread one-per-frame right after the picker opens (see
// computeSlotDetail/deferPrecomputeDetails) — updateDetailPanel then just
// formats already-computed numbers instead of re-parsing anything.
class PickerActivity : public brls::Activity {
public:
    brls::View* createContentView() override;
    void onContentAvailable() override;
    void onResume() override;
    void willDisappear(bool resetState = false) override;
    ~PickerActivity() override;

private:
    struct PendingThumbnail {
        brls::Image* image = nullptr;
        std::string captionPath;
    };

    struct SlotDetail {
        bool loaded = false;
        std::string playtime;
        uint32_t maxHearts = 0;
        uint32_t maxStamina = 0;
        uint32_t rupees = 0;
        float maxBattery = 0.0f;
        int korokDone = 0;
        int korokTotal = 0;
        int shrineDone = 0;
        int shrineTotal = 0;
        int lightrootDone = 0;
        int lightrootTotal = 0;
    };

    brls::Box* slotList_ = nullptr;
    bool slotsPopulated_ = false;
    std::vector<PendingThumbnail> pendingThumbnails_;
    std::vector<totk::SaveSlotInfo> slots_;
    std::vector<brls::View*> slotRows_;
    std::vector<SlotDetail> slotDetails_;

    GlowPanel* detailPanel_ = nullptr;
    brls::Image* detailThumb_ = nullptr;
    brls::Label* detailNameLabel_ = nullptr;  // location name — the slot's primary heading
    brls::Label* detailDateLabel_ = nullptr;
    brls::Label* detailAutosaveLabel_ = nullptr;
    brls::Label* detailPlaytimeLabel_ = nullptr;
    brls::Box* detailHeartsRow_ = nullptr;
    brls::Box* detailStaminaRow_ = nullptr;
    brls::Label* detailRupeesLabel_ = nullptr;
    brls::Label* detailBatteryLabel_ = nullptr;
    brls::Label* detailKorokLabel_ = nullptr;
    brls::Label* detailShrineLabel_ = nullptr;
    brls::Label* detailLightrootLabel_ = nullptr;
    size_t detailLoadedIndex_ = static_cast<size_t>(-1);
    brls::Event<brls::View*>::Subscription focusSubscription_;
    bool focusSubscribed_ = false;

    void populateSlots();
    brls::View* firstSlotRow();
    brls::Box* makeSlotRow(const totk::SaveSlotInfo& slot, size_t index);
    void deferLoadThumbnails(size_t index);
    void updateDetailPanel(size_t index);
    void computeSlotDetail(size_t index);
    void deferPrecomputeDetails(size_t index);
};

}  // namespace totk::save_editor
