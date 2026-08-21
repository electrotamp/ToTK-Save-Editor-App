#pragma once

#include <functional>
#include <string>
#include <vector>

#include <borealis.hpp>

namespace totk {
struct ItemEntry;
}

namespace totk::ui {

class ItemIconTile;

class ItemPickerActivity : public brls::Activity {
public:
    ItemPickerActivity(std::string category, std::string title, std::function<void()> onChanged = nullptr,
                       std::function<void(const std::string& itemId)> onSelected = nullptr);
    ~ItemPickerActivity() override;

    static ItemPickerActivity* fusePicker(std::function<void(const std::string& fuseId)> onSelected);

    brls::View* createContentView() override;
    void willDisappear(bool resetState = false) override;

private:
    std::string category_;
    std::string title_;
    std::function<void()> onChanged_;
    std::function<void(const std::string& itemId)> onSelected_;
    bool selectOnly_ = false;

    int selectedIndex_ = 0;
    int gridColumns_ = 5;
    int gridImageLoadGeneration_ = 0;
    int tileBuildIndex_ = 0;
    bool inputsBlocked_ = false;
    bool gridBuildComplete_ = false;
    bool gridImagesLoaded_ = false;
    const std::vector<totk::ItemEntry>* catalog_ = nullptr;
    std::vector<totk::ItemEntry> filteredCatalog_;

    brls::Box* root_ = nullptr;
    brls::Image* detailIcon_ = nullptr;
    brls::Label* detailName_ = nullptr;
    brls::Label* detailSubtitle_ = nullptr;
    brls::Label* detailHint_ = nullptr;
    brls::ScrollingFrame* gridScroll_ = nullptr;
    brls::Box* gridContent_ = nullptr;
    std::vector<brls::Box*> gridRows_;
    std::vector<ItemIconTile*> tiles_;
    std::vector<brls::View*> gridTiles_;
    brls::Event<brls::View*>::Subscription focusSubscription_;

    void buildFilteredCatalog();
    void selectIndex(int index);
    void updateDetailPanel(int index);
    void scrollToTile(brls::View* tile);
    void focusSelectedTile(bool releaseInputsWhenLanded = false);
    bool tryAddSelectedItem();
    bool tryConfirmSelection();
    void deferLoadTileImages(int generation, std::function<void()> onComplete = nullptr);
    void rebindCachedTileCallbacks();
    void finishCachedGridRestore(int loadGeneration, bool imagesLoaded);
    void stashGridInCache();
    bool tryRestoreCachedGrid(int loadGeneration);
    int buildTileBatch(int generation, int maxTiles);
    void finishPickerGridBuild(int generation, bool releaseInputsWhenLanded = false);
    void unblockPickerInputs();
    void scheduleBuildTilesBatch(int generation);
    void rebuildGrid();
};

}  // namespace totk::ui
