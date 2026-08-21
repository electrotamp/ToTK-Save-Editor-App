#pragma once

#include <borealis.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace totk::ui {
class BoundedNumericCell;
}

namespace totk::save_editor {

// Single persistent Activity hosting every post-picker tab (Stats, Weapons,
// Shields, Bows, Armors, Materials, Food, Zonai Devices, Key Items, Horses).
//
// Earlier, each tab was its own brls::Activity, and L/R switched tabs by
// popping the current one and pushing the next (see tab_order.cpp's git
// history). That worked, but meant every tab switch tore down and rebuilt
// the whole Activity — AppletFrame, wisp overlay included — and, because
// Borealis's popActivity() always calls onResume() on whatever activity sits
// beneath the one being popped, every switch also briefly (if invisibly, see
// Borealis's View::hide with animated=false) resurfaced PickerActivity.
//
// This class instead stays on the activity stack for the whole time the user
// is inside the editor: createContentView() builds the AppletFrame, the
// wisp overlay, and an empty contentRoot_ ONCE; switchToTab() then only
// clears and rebuilds contentRoot_ — the frame chrome and wisp survive every
// switch untouched. Popping this Activity (Back from any tab) is the only
// time the picker is genuinely revealed again.
class TabHostActivity : public brls::Activity {
public:
    explicit TabHostActivity(std::string initialTabId);

    brls::View* createContentView() override;
    void onContentAvailable() override;
    void onResume() override;
    void willDisappear(bool resetState = false) override;
    ~TabHostActivity() override;

private:
    std::string initialTabId_;
    std::string currentTabId_;

    brls::Box* root_ = nullptr;         // persistent: wisp attached here once
    brls::Box* contentRoot_ = nullptr;  // cleared + rebuilt on every switchToTab

    // AppletFrame's own title label is left-aligned in the header (see
    // wireFrameChrome's comment) — sat right on top of the wallpaper's
    // logo. Built once in createContentView() as a separate label pinned
    // across the whole header and center-aligned, so it never competes with
    // the logo's corner; wireFrameChrome() just updates its text per switch
    // and leaves the frame's own title empty.
    brls::Label* tabTitleLabel_ = nullptr;

    // Shared grid state (weapons/shields/bows/armors/materials/food/devices/key/horses).
    std::vector<brls::View*> tiles_;
    brls::Image* headerIcon_ = nullptr;
    brls::Label* headerName_ = nullptr;
    brls::Label* headerSubtitle_ = nullptr;
    brls::Image* headerFuseIcon_ = nullptr;
    brls::Label* headerFuseLabel_ = nullptr;  // equipment (weapons/shields/bows) tabs only

    // Stats form state.
    brls::Label* versionLabel_ = nullptr;
    brls::Label* playtimeLabel_ = nullptr;
    brls::Label* mapPinsLabel_ = nullptr;
    totk::ui::BoundedNumericCell* rupeesCell_ = nullptr;
    brls::SelectorCell* heartsCell_ = nullptr;
    brls::SelectorCell* staminaCell_ = nullptr;
    brls::SelectorCell* batteryCell_ = nullptr;
    totk::ui::BoundedNumericCell* ponyCell_ = nullptr;
    totk::ui::BoundedNumericCell* pouchWeaponsCell_ = nullptr;
    totk::ui::BoundedNumericCell* pouchBowsCell_ = nullptr;
    totk::ui::BoundedNumericCell* pouchShieldsCell_ = nullptr;
    totk::ui::BoundedNumericCell* posXCell_ = nullptr;
    totk::ui::BoundedNumericCell* posYCell_ = nullptr;
    totk::ui::BoundedNumericCell* posZCell_ = nullptr;

    brls::Event<brls::View*>::Subscription focusSubscription_;
    bool focusSubscribed_ = false;

    // Bumped once at the top of every switchToTab(), including a switch back
    // to the same tab. Each builder's deferred (brls::sync) focus-set
    // callback captures the value current at schedule time and re-checks it
    // before touching any pointer — if a newer switch has since cleared and
    // rebuilt contentRoot_ (e.g. from spamming L/R faster than a frame),
    // the callback recognizes it's stale and bails instead of touching
    // tiles_/cell pointers that may now be dangling.
    uint32_t buildGeneration_ = 0;

    // Borealis's View::registerAction only ever replaces an existing binding
    // on the SAME button (see view.cpp) — it never removes one. Since Stats
    // has no Add button, and the first/last tab in the order has no L or R
    // neighbor, each of those three bindings has to be explicitly unregistered
    // when the incoming tab doesn't want it, or the outgoing tab's handler
    // (and its stale captured neighbor id) would keep firing.
    brls::ActionIdentifier addActionId_ = 0;
    bool addActionRegistered_ = false;
    brls::ActionIdentifier prevActionId_ = 0;
    bool prevActionRegistered_ = false;
    brls::ActionIdentifier nextActionId_ = 0;
    bool nextActionRegistered_ = false;

    void switchToTab(const std::string& tabId);
    void wireFrameChrome(const std::string& tabId);
    void setAddAction(bool enabled, const std::string& label, brls::ActionListener listener);
    void setNeighborActions(const std::string& tabId);

    // Shared by both grid kinds: subscribes once (lazily, on first grid
    // build) for the activity's whole lifetime. Its callback reads
    // currentTabId_ live rather than capturing a category, so — unlike the
    // old per-tab Activities' subscriptions, each scoped to one Activity
    // instance's fixed category — this one stays correct across every future
    // tab switch without ever needing to be torn down and re-subscribed.
    void ensureGridFocusSubscription();

    // Equipment grid (weapons/shields/bows).
    void buildEquipmentGrid(const std::string& category);
    void updateEquipmentHeader(size_t index);
    void openEquipmentAddPicker(const std::string& category);

    // Pouch grid (armors/materials/food/devices/key/horses).
    void buildPouchGrid(const std::string& category);
    void updatePouchHeader(size_t index);
    void openPouchAddPicker(const std::string& category);
    size_t pouchItemCount(const std::string& category) const;
    size_t pouchCapacity(const std::string& category) const;
    bool pouchAddItem(const std::string& category, const std::string& id);

    // Stats form.
    void buildStatsForm();
    void refreshStatsFromSave();
    void applyStatsToSave();

    // Autobuild tab: HyruleWorks/SD-import entry point + Favorites slot list.
    void buildAutobuildTab();

    // Credits tab: static, read-only attribution list (see ATTRIBUTION.md).
    void buildCreditsTab();
};

// Pushes the one TabHostActivity for the editor session, landing on
// `initialTabId` (NONE transition). Call once, from the picker's slot-open
// success path — every subsequent tab switch stays inside that same
// instance via L/R, never touching the activity stack again.
void openTabHost(const std::string& initialTabId);

}  // namespace totk::save_editor
