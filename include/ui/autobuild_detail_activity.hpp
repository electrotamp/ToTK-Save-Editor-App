#pragma once

#include <borealis.hpp>

#include "net/hyruleworks_client.hpp"

namespace totk::ui {

// Build detail view: full image gallery, creator, description, and an
// "Import to Slot" action that downloads the blueprint and hands off to
// pushAutobuildSlotPicker() to choose the target Favorites slot.
class AutobuildDetailActivity : public brls::Activity {
public:
    explicit AutobuildDetailActivity(int buildId);

    brls::View* createContentView() override;
    void onContentAvailable() override;

private:
    void loadDetail();
    void showImage(size_t index);
    void doImport();

    int buildId_;
    int generation_ = 0;
    totk::net::HyruleWorksBuildDetail detail_;
    size_t currentImageIndex_ = 0;

    brls::Image* image_ = nullptr;
    brls::Label* titleLabel_ = nullptr;
    brls::Label* creatorLabel_ = nullptr;
    brls::Label* descriptionLabel_ = nullptr;
    brls::Label* statusLabel_ = nullptr;
    brls::Label* imageCounterLabel_ = nullptr;
    brls::View* focusTarget_ = nullptr;
};

void pushAutobuildDetailActivity(int buildId);

}  // namespace totk::ui
