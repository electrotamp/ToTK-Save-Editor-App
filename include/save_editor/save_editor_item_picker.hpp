#pragma once

#include <borealis.hpp>
#include <functional>
#include <string>
#include <vector>

namespace totk {
struct ItemEntry;
}

namespace totk::save_editor {

// Minimal icon-grid picker activity for the editor: either a single
// save-editor catalog category (add weapon) or a mixed-category item set
// (fuse material — spans weapons/bows/shields/materials/food). Built
// synchronously in one pass, no deferred batching or grid caching — mirrors
// WeaponsActivity::buildGrid(). Icons come from the atlas (ItemDatabase's
// per-category or per-id path candidates); a mixed grid resolves each
// item's category by its id since ItemEntry doesn't carry one.
class ItemPickerActivity : public brls::Activity {
public:
    static ItemPickerActivity* forCategory(std::string category, std::string title,
                                            std::function<void(const std::string& id)> onSelected);
    static ItemPickerActivity* forMixedGrid(std::string title, std::vector<totk::ItemEntry> items,
                                            std::function<void(const std::string& id)> onSelected);

    brls::View* createContentView() override;
    void willDisappear(bool resetState = false) override;
    ~ItemPickerActivity() override;

private:
    ItemPickerActivity(std::string title, std::string category, std::vector<totk::ItemEntry> items,
                       std::function<void(const std::string& id)> onSelected);

    std::string title_;
    std::string category_;  // empty => mixed-category grid, resolve per item id
    std::vector<totk::ItemEntry> baseItems_;  // full, unfiltered catalog passed in at construction
    std::vector<totk::ItemEntry> items_;      // baseItems_ after the current search/sort is applied
    std::function<void(const std::string& id)> onSelected_;

    std::string searchQuery_;

    brls::View* buildIconGrid();
    void populateGrid(brls::Box* content);
    void rebuildGrid();
    void applyFilterAndSort();
    void openSearchKeyboard();
    void refreshHintText();
    void updateClearSearchAction();
    void selectItem(const std::string& id);
    void updateDetailForIndex(size_t index);

    brls::Image* detailIcon_ = nullptr;
    brls::Label* detailName_ = nullptr;
    brls::Label* detailSubtitle_ = nullptr;
    brls::Label* detailHint_ = nullptr;
    brls::Label* hintLabel_ = nullptr;
    brls::Box* searchBar_ = nullptr;
    brls::Label* searchBarLabel_ = nullptr;
    brls::AppletFrame* frame_ = nullptr;
    brls::ActionIdentifier clearSearchActionId_ = 0;
    bool clearSearchRegistered_ = false;
    brls::Box* gridContent_ = nullptr;
    std::vector<brls::View*> tiles_;
    brls::Event<brls::View*>::Subscription focusSubscription_;
    bool focusSubscribed_ = false;
};

}  // namespace totk::save_editor
