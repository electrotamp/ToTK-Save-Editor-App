#pragma once

#include <nanovg.h>

namespace totk::ui {

struct AppThemeColors {
    NVGcolor tileBackground = nvgRGBA(255, 255, 255, 12);
    NVGcolor mutedText = nvgRGB(160, 160, 160);
    NVGcolor fusePanelBackground = nvgRGBA(255, 255, 255, 16);
    NVGcolor detailPanelBackground = nvgRGBA(255, 255, 255, 16);
    NVGcolor accentHintText = nvgRGB(130, 190, 150);
    NVGcolor dangerText = nvgRGB(220, 90, 90);
};

class AppTheme {
public:
    static AppTheme& instance();

    const AppThemeColors& colors() const { return colors_; }
    void setColors(AppThemeColors colors);
    void refreshThemedViews();

private:
    AppThemeColors colors_;
};

}  // namespace totk::ui
