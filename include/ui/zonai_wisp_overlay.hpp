#pragma once

#include <borealis/core/box.hpp>
#include <borealis/core/view.hpp>
#include <string>
#include <vector>

#include "ui/zonai_wisp_simulator.hpp"

namespace brls {
class AppletFrame;
}

namespace totk::ui {

class ZonaiWispOverlay : public brls::View {
public:
    ZonaiWispOverlay();

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;
    void willAppear(bool resetState = false) override;

    static brls::View* create();

private:
    struct ScreenPoint {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float alpha = 0.0f;
    };

    bool isOnScreen(float px, float py, float x, float y, float width, float height) const;

    void buildScreenRibbon(const std::vector<float>& xs, const std::vector<float>& ys, int pointCount, int subdiv,
                           float baseAlpha, float headWidth, float x, float y, float width, float height,
                           const WispProfile& profile);

    void drawWisp(NVGcontext* vg, float x, float y, float width, float height, const WispEntity& wisp,
                  const WispProfile& profile, WispSimulator& simulator);
    void drawFragment(NVGcontext* vg, float x, float y, float width, float height, const WispFragment& fragment,
                      const WispProfile& profile, WispSimulator& simulator);

    std::vector<WispPoint2> sampledPath_;
    std::vector<float> scratchPx_;
    std::vector<float> scratchPy_;
    std::vector<ScreenPoint> screenRibbon_;
};

// Adds a full-screen wisp layer behind existing content. Returns nullptr if wisps are disabled.
ZonaiWispOverlay* attachWispOverlay(brls::Box* parent);

// Full-bleed background image, drawn with a raw nvgImagePattern rather than
// brls::Image: Image unconditionally registers a custom Yoga measure
// function (see image.cpp's constructor), and Borealis's own source notes
// that measure-function nodes deviate from normal percentage-based sizing —
// in practice this left a 100%/100%-sized Image capped short of its actual
// box under every ImageScalingType tried. A plain View has no measure
// function, so its percentage sizing resolves the same ordinary way
// ZonaiWispOverlay's already does (proven to fill correctly), and draw()
// always receives the view's real on-screen (x, y, width, height) directly —
// there's no separate sizing step for those to diverge from.
class EditorWallpaper : public brls::View {
public:
    EditorWallpaper();

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

private:
    int texture_ = 0;
    bool loadAttempted_ = false;
};

// Adds the app's background wallpaper image plus the wisp layer, in the
// correct paint order so the wisps stay visible over the artwork instead of
// being hidden beneath it: whichever of the two is added to `parent` last
// via addView(view, 0) ends up as the true index 0 (see Box::draw's forward
// iteration — index 0 paints first, so it ends up furthest back), so this
// attaches the wisp overlay first (temporarily index 0) and the wallpaper
// second, explicitly at index 0, bumping the wisp to index 1 — wallpaper on
// the very bottom, wisps painted just above it, all real content painted
// above both. Call this everywhere attachWispOverlay was called directly, so
// the wallpaper (and thus the background) is consistent across every editor
// screen, not just the ones that happen to add it themselves.
void attachEditorBackground(brls::Box* parent);

// Clears `frame`'s own title (always left-aligned by AppletFrame's own
// internal header layout, with no built-in way to recenter it — see
// tab_host_activity.cpp's matching comment) and shows `text` instead via a
// separate label centered across the whole header, so it never collides with
// the wallpaper's logo in the header's top-left corner. Every editor screen
// that sets a frame title should go through this instead of frame->setTitle()
// directly, now that they all show the wallpaper.
void setCenteredHeaderTitle(brls::AppletFrame* frame, const std::string& text);

}  // namespace totk::ui
