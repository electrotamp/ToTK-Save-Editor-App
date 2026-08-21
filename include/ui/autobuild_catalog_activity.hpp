#pragma once

#include <string>
#include <vector>

#include <borealis.hpp>

#include "net/hyruleworks_client.hpp"

namespace totk::ui {

// Paginated HyruleWorks catalog browser: one page (30 cards, per the site's
// own listing) at a time, with search. Selecting a card pushes
// AutobuildDetailActivity for that build's id.
class AutobuildCatalogActivity : public brls::Activity {
public:
    AutobuildCatalogActivity();

    brls::View* createContentView() override;

private:
    void loadPage(int page);
    void renderBuilds();
    void loadThumbnails(int generation);
    void openSearch();

    int page_ = 1;
    std::string searchQuery_;
    std::vector<totk::net::HyruleWorksBuildSummary> builds_;
    int loadGeneration_ = 0;
    bool hasNextPage_ = false;

    brls::Box* root_ = nullptr;
    brls::Label* statusLabel_ = nullptr;
    brls::ScrollingFrame* listFrame_ = nullptr;
    brls::Box* listBox_ = nullptr;
    std::vector<brls::Image*> rowImages_;
};

void pushAutobuildCatalogActivity();

}  // namespace totk::ui
