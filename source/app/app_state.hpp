#pragma once

#include <string>
#include <vector>

#include "save/save_editor.hpp"
#include "ui/icon_grid_metrics.hpp"

namespace totk {

class AppState {
public:
    static AppState& instance();

    SaveEditor& editor() { return editor_; }
    const SaveEditor& editor() const { return editor_; }

    std::string saveRoot() const { return saveRoot_; }
    void setSaveRoot(const std::string& path) { saveRoot_ = path; }

    bool hasLoadedSave() const { return editor_.isLoaded(); }
    std::string currentSavePath() const { return currentSavePath_; }
    void setCurrentSavePath(const std::string& path) { currentSavePath_ = path; }

    int gridZoomPercent() const { return gridZoomPercent_; }
    void setGridZoomPercent(int percent) { gridZoomPercent_ = ui::clampIconGridZoom(percent); }
    bool adjustGridZoom(int direction);
    ui::IconGridMetrics iconGridMetrics() const { return ui::iconGridMetricsForZoom(gridZoomPercent_); }

    void preloadSaveSlots();
    bool hasPreloadedSaveSlots() const { return saveSlotsPreloaded_; }
    const std::vector<SaveSlotInfo>& preloadedSaveSlots() const { return preloadedSlots_; }

private:
    AppState() = default;
    SaveEditor editor_;
    std::string saveRoot_ = SaveEditor::defaultSaveRoot();
    std::string currentSavePath_;
    int gridZoomPercent_ = ui::kIconGridZoomDefault;
    std::vector<SaveSlotInfo> preloadedSlots_;
    bool saveSlotsPreloaded_ = false;
};

}  // namespace totk
