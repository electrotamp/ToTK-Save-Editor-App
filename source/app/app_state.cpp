#include "app/app_state.hpp"

#include "util/perf_trace.hpp"

#if defined(__SWITCH__)
#include "platform/switch_save_mount.hpp"
#endif

namespace totk {

AppState& AppState::instance() {
    static AppState state;
    return state;
}

bool AppState::adjustGridZoom(int direction) {
    if (direction == 0) return false;

    const int next = gridZoomPercent_ + direction * ui::kIconGridZoomStep;
    if (next < ui::kIconGridZoomMin || next > ui::kIconGridZoomMax) {
        return false;
    }

    gridZoomPercent_ = next;
    return true;
}

void AppState::preloadSaveSlots() {
    TOTK_PERF_SCOPE("startup.preloadSaveSlots");
#if defined(__SWITCH__)
    if (!SwitchSaveMount::isMounted()) {
        SwitchSaveMount::initialize();
        if (SwitchSaveMount::isMounted()) {
            saveRoot_ = SwitchSaveMount::saveRoot();
        }
    }
#endif

    preloadedSlots_ = SaveEditor::scanSaveDirectory(saveRoot_);
    saveSlotsPreloaded_ = true;
}

}  // namespace totk
