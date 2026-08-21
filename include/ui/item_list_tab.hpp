#pragma once

#include <borealis.hpp>
#include <string>
#include <vector>

#include "ui/icon_grid_metrics.hpp"
#include "ui/pouch_item_display.hpp"

namespace totk::ui {

class ItemFocusHeader;
class ItemAddTile;
class ItemIconTile;

class ItemListTab : public brls::Box {
public:
    ItemListTab(const std::string& category, const std::string& title);
    static brls::View* createWeapons();
    static brls::View* createBows();
    static brls::View* createShields();
    static brls::View* createArmors();
    static brls::View* createMaterials();
    static brls::View* createFood();
    static brls::View* createDevices();
    static brls::View* createKeyItems();
    static brls::View* createHorses();

    void refresh();
    void onItemAdded();
    void onItemChanged();
    void onItemDeleted(int deletedIndex);
    void focusInitialSelection();
    void activatePageFocus();

    void willAppear(bool resetState = false) override;
    void willDisappear(bool resetState = false) override;
    ~ItemListTab() override;

private:
    void logFocusTrace(const char* event) const;

    std::string category_;
    std::string title_;
    int selectedIndex_ = 0;
    int gridColumns_ = 5;
    int layoutZoomPercent_ = kIconGridZoomDefault;

    ItemFocusHeader* focusHeader_ = nullptr;
    brls::Label* countLabel_ = nullptr;
    brls::ScrollingFrame* gridScroll_ = nullptr;
    brls::Box* gridContent_ = nullptr;
    brls::Box* focusAnchor_ = nullptr;
    ItemAddTile* addTile_ = nullptr;
    ItemAddTile* arrowAddTile_ = nullptr;
    std::vector<ItemIconTile*> tiles_;
    std::vector<brls::View*> gridTiles_;
    brls::Event<brls::View*>::Subscription focusSubscription_;
    brls::ActionIdentifier gridActionId_ = 0;
    int gridImageLoadGeneration_ = 0;
    int interactionEpoch_ = 0;
    int focusRestoreEpoch_ = 0;
    int focusRestoreAttempts_ = 0;
    bool pendingFocusRestore_ = false;
    bool gridRebuildInProgress_ = false;
    bool iconsLoadPending_ = false;
    bool deferredRebuildPending_ = false;

    void createFocusAnchor();
    void redirectFocusToPagerSink();
    void redirectFocusAwayFromGrid();
    bool isForegroundTab();
    bool isUnderOverlayActivity() const;
    void finishGridRebuild();
    void rebuild();
    void deferLoadTileImages(int generation);
    void ensureTileImagesLoaded();
    void rebuildGrid();
    void updateGridLayout();
    void updateCountLabel();
    void releaseGridFocus();
    void restoreGridFocus();
    void scheduleFocusRestore();
    void attemptFocusRestore();
    void completeFocusRestore();
    void selectIndex(int index);
    void setFocusDetail(const totk::ItemDisplayInfo& info);
    void updateFocusDetailForView(brls::View* view);
    void openItemEditor(int index);
    void openItemPicker();
    void wireGridNavigation();
    void scrollToTile(brls::View* tile);
    bool supportsAdd() const;
    bool canAddMore() const;
    bool isBowsTab() const;
    int gridSlotCount() const;
    totk::ItemDisplayInfo displayInfoForGridSlot(int slot) const;
    void handleGridSlotAction(int slot);
    void addArrowsFromTab();
    std::string addTileCaption() const;
    std::string databaseCategory() const;
    int itemCount() const;
};

}  // namespace totk::ui
