#pragma once

#include <functional>
#include <string>
#include <vector>

#include <borealis.hpp>

#include "ui/icon_grid_metrics.hpp"

namespace totk::ui {

class ItemIconTile : public brls::Box {
public:
    ItemIconTile();

    void applyLayout(const IconGridMetrics& metrics);
    void applyThemeColors();
    void configure(const std::string& badgeText, const std::vector<std::string>& iconPaths,
                   const std::vector<std::string>& fuseIconPaths, std::function<void()> onFocus,
                   std::function<void()> onActivate);
    void rebindCallbacks(std::function<void()> onFocus, std::function<void()> onActivate);
    void loadImages();
    bool needsImageLoad() const;
    void refreshImageBounds();

    void onFocusGained() override;

private:
    brls::Label* badge_ = nullptr;
    brls::Box* iconWrap_ = nullptr;
    brls::Image* baseIcon_ = nullptr;
    std::vector<std::string> iconPaths_;
    std::string loadedIconPath_;
    std::function<void()> onFocus_;
    std::function<void()> onActivate_;
    float iconSize_ = 128.0f;
    bool clickActionRegistered_ = false;

    void applyIconLayout(float iconSize);
};

struct IconGridNavigationOptions {
    bool allowExitLeft = false;
};

void wireIconGridNavigation(const std::vector<brls::View*>& tiles, int columns,
                            IconGridNavigationOptions options = {});

// Loads tile icons in small batches across frames to avoid stalling the UI.
void batchLoadIconTileImages(const std::vector<ItemIconTile*>& tiles, int generation, int* activeGeneration,
                             brls::Box* gridContent, const char* label = nullptr, size_t startIndex = 0,
                             std::function<void()> onComplete = nullptr);

class ItemAddTile : public brls::Box {
public:
    ItemAddTile();

    void applyLayout(const IconGridMetrics& metrics);
    void applyThemeColors();
    void configure(std::function<void()> onFocus, std::function<void()> onActivate);

    void onFocusGained() override;

private:
    brls::Label* plus_ = nullptr;
    std::function<void()> onFocus_;
    std::function<void()> onActivate_;
};

}  // namespace totk::ui
